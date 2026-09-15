#include "acevo/ui/menu_refresh_fix.h"
#include "acevo/core/code_patch.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"

// Read from AssettoCorsaEVO.exe 0.9.1 and cohtml.WindowsDesktop.dll 1.61.0.3, measured with the UI
// probe on 2026-09-14.
//
// Every frame GameUi::PostFrame lists the UI surfaces to update, the main menu or HUD surface first
// (GameUi+0xB0, pushed first by the list builder at 0xDE46A0) and then the car's dashboard displays.
// In the main menu (+0x555) and the pause menu (+0x556) it updates all of them. Otherwise it keeps one,
// in turn (the counter at +0x1EC), so with the two displays of a session the menu updates one frame in
// three. In the pit lane menu, in-session settings and vehicle setup at 90 fps that is a 30 fps menu,
// which is what it feels like, and every hover or slider step waits for the menu's turn.
//
// The stub at 0xDE37B7 keeps the main surface every frame and passes the turn around the displays only,
// and only while the main surface shows a menu page. On the HUD it does what the game does, because
// updating the HUD three times as often while driving would cost frame time. Which page is loaded comes
// from Cohtml's URL loader (0x46B990, reached by View::LoadURL and by page navigation from script),
// whose entry is redirected to a stub that looks at the URL and sets a byte the rotation stub reads.

static const DWORD kGameTimeDateStamp = 0x6A9EC72A;
static const DWORD kGameSizeOfImage = 0x06CDD000;
static const DWORD kCohtmlTimeDateStamp = 0x675439C7;
static const DWORD kCohtmlSizeOfImage = 0x0070F000;

struct Region {
    uint32_t rva;
    uint32_t length;
    uint64_t fnv1a64;
    const char* what;
};

static const Region kRotation = { 0xDE379B, 0x5C, 0xC79C3BCD9EBE1EC4ull, "the UI surface rotation" };
static const Region kSurfaceList = { 0xDE46C8, 0x2B, 0x4F5762DCD371D1A5ull, "the UI surface list builder" };
static const Region kEveryView = { 0xDE38B1, 8, 0x10061AD7224D88A2ull, "the every surface path" };
static const Region kLoadUrl = { 0x46B990, 16, 0xF911DFAD14A7CAA7ull, "Cohtml's URL loader" };

static const uint32_t kRvaRotationPick = 0xDE37B7;       // mov rcx, [rsp+0x38], five bytes
static const uint32_t kRvaAfterRotationPick = 0xDE37BC;
static const uint32_t kRvaEveryView = 0xDE38B1;

// Pages the main surface shows as a menu. hud.html is the HUD, anything else leaves the state alone.
static const char* const kMenuPages[] = {
    "intro.html", "menu.html", "singleplayer.html", "multiplayer.html", "ingame.html", "settings.html",
    "vehicles.html", "paintshop.html", "partshop.html", "drivercenter.html", "drivingacademy.html",
    "gallery.html", "replay.html", "simgrid.html",
};

static BYTE* g_menuShown = nullptr;     // one byte on a read and write page, read by the rotation stub

//   cmp byte ptr [rip+menu shown], 0 / je original
//   mov rbx, [rsp+0x30] / mov rcx, [rsp+0x38] / sub rcx, rbx / sar rcx, 3     the surface count
//   cmp rcx, 2 / jb original
//   lea rax, [r14+0xB0] / cmp [rbx], rax / jne original                       the main surface first
//   dec rcx / movsxd rax, dword ptr [r14+0x1EC] / inc rax / xor edx, edx / div rcx
//   mov [r14+0x1EC], edx                                                      the display whose turn it is
//   mov rax, [rbx+rdx*8+8] / mov [rbx+8], rax / lea rax, [rbx+0x10] / mov [rsp+0x38], rax
//   jmp every surface path
//   original: mov rcx, [rsp+0x38] / jmp back to the game's pick
// The list keeps its own buffer and capacity, only its end moves, so the game frees it as it would.
static const BYTE kRotationStub[] = {
    0x80, 0x3D, 0, 0, 0, 0, 0x00,
    0x74, 0x5C,
    0x48, 0x8B, 0x5C, 0x24, 0x30,
    0x48, 0x8B, 0x4C, 0x24, 0x38,
    0x48, 0x29, 0xD9,
    0x48, 0xC1, 0xF9, 0x03,
    0x48, 0x83, 0xF9, 0x02,
    0x72, 0x45,
    0x49, 0x8D, 0x86, 0xB0, 0x00, 0x00, 0x00,
    0x48, 0x39, 0x03,
    0x75, 0x39,
    0x48, 0xFF, 0xC9,
    0x49, 0x63, 0x86, 0xEC, 0x01, 0x00, 0x00,
    0x48, 0xFF, 0xC0,
    0x31, 0xD2,
    0x48, 0xF7, 0xF1,
    0x41, 0x89, 0x96, 0xEC, 0x01, 0x00, 0x00,
    0x48, 0x8B, 0x44, 0xD3, 0x08,
    0x48, 0x89, 0x43, 0x08,
    0x48, 0x8D, 0x43, 0x10,
    0x48, 0x89, 0x44, 0x24, 0x38,
    0xFF, 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x48, 0x8B, 0x4C, 0x24, 0x38,
    0xFF, 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
static const size_t kRotationStubFlagDispAt = 2;
static const size_t kRotationStubFlagNextIp = 7;
static const size_t kRotationStubEveryViewAt = 93;
static const size_t kRotationStubBackAt = 112;

// The URL loader starts with mov rax, rsp / push rbp / push rbx, five bytes, which the jump to this stub
// replaces. The stub keeps the argument registers, hands the hook the view and the URL, replays the
// moved instructions and jumps back past them.
static const BYTE kUrlStub[] = {
    0x51,                                       // push rcx
    0x52,                                       // push rdx
    0x41, 0x50,                                 // push r8
    0x41, 0x51,                                 // push r9
    0x48, 0x83, 0xEC, 0x28,                     // sub rsp, 0x28
    0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,         // mov rax, OnLoadUrl
    0xFF, 0xD0,                                 // call rax
    0x48, 0x83, 0xC4, 0x28,                     // add rsp, 0x28
    0x41, 0x59,                                 // pop r9
    0x41, 0x58,                                 // pop r8
    0x5A,                                       // pop rdx
    0x59,                                       // pop rcx
    0x48, 0x8B, 0xC4,                           // mov rax, rsp
    0x55,                                       // push rbp
    0x53,                                       // push rbx
    0xFF, 0x25, 0, 0, 0, 0,                     // jmp [rip]
    0, 0, 0, 0, 0, 0, 0, 0,                     // the URL loader + 5
};
static const size_t kUrlStubHookAt = 12;
static const size_t kUrlStubBackAt = sizeof kUrlStub - 8;

static bool EndsWithPage(const char* url, size_t length, const char* page)
{
    size_t pageLength = strlen(page);
    if (length < pageLength + 1 || url[length - pageLength - 1] != '/') return false;
    return _strnicmp(url + length - pageLength, page, pageLength) == 0;
}

// Script hands the loader whatever it navigated to, so the URL is read defensively and only a page this
// knows changes the state.
static void OnLoadUrl(void*, const char* url)
{
    char path[512];
    size_t length = 0;
    __try {
        if (!url) return;
        for (; length < sizeof path - 1 && url[length] && url[length] != '?' && url[length] != '#'; ++length) path[length] = url[length];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    path[length] = 0;

    if (EndsWithPage(path, length, "hud.html")) {
        *g_menuShown = 0;
        return;
    }
    for (const char* page : kMenuPages) {
        if (!EndsWithPage(path, length, page)) continue;
        *g_menuShown = 1;
        return;
    }
}

static bool EncodeJump(const BYTE* from, const BYTE* destination, BYTE* out)
{
    int64_t rel = destination - (from + 5);
    if (rel > INT32_MAX || rel < INT32_MIN) return false;
    int32_t value = (int32_t)rel;
    out[0] = 0xE9;
    memcpy(out + 1, &value, 4);
    return true;
}

static bool WriteCode(BYTE* at, const BYTE* code, size_t length)
{
    DWORD old = 0;
    if (!VirtualProtect(at, length, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(at, code, length);
    DWORD ignored = 0;
    VirtualProtect(at, length, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), at, length);
    return true;
}

static bool Matches(BYTE* base, const Region& region)
{
    if (Fnv1a64(base + region.rva, region.length) == region.fnv1a64) return true;
    Log("[menus] %s at rva 0x%07X is not the code this was written against, nothing patched", region.what, (unsigned)region.rva);
    return false;
}

void InstallMenuRefreshFix()
{
    if (!g_cfg.uiMenuRefreshFix) return;

    BYTE* game = (BYTE*)GetModuleHandleW(nullptr);
    BYTE* cohtml = (BYTE*)GetModuleHandleW(L"cohtml.WindowsDesktop.dll");
    auto gameNt = (IMAGE_NT_HEADERS64*)(game + ((IMAGE_DOS_HEADER*)game)->e_lfanew);
    if (gameNt->FileHeader.TimeDateStamp != kGameTimeDateStamp || gameNt->OptionalHeader.SizeOfImage != kGameSizeOfImage) {
        Log("[menus] this is not the game build the menu refresh fix was written for, nothing patched");
        return;
    }
    if (!cohtml) {
        Log("[menus] cohtml.WindowsDesktop.dll is not loaded, nothing patched");
        return;
    }
    auto cohtmlNt = (IMAGE_NT_HEADERS64*)(cohtml + ((IMAGE_DOS_HEADER*)cohtml)->e_lfanew);
    if (cohtmlNt->FileHeader.TimeDateStamp != kCohtmlTimeDateStamp || cohtmlNt->OptionalHeader.SizeOfImage != kCohtmlSizeOfImage) {
        Log("[menus] this is not the Cohtml build the menu refresh fix was written for, nothing patched");
        return;
    }
    if (!Matches(game, kRotation) || !Matches(game, kSurfaceList) || !Matches(game, kEveryView) || !Matches(cohtml, kLoadUrl)) return;

    // Two stubs a jump can reach from each module, and a data page for the state byte beside each.
    const size_t page = 0x1000;
    BYTE* gameCave = AllocNear(game + kRvaRotationPick, 2 * page);
    BYTE* cohtmlCave = AllocNear(cohtml + kLoadUrl.rva, page);
    if (!gameCave || !cohtmlCave) {
        if (gameCave) VirtualFree(gameCave, 0, MEM_RELEASE);
        if (cohtmlCave) VirtualFree(cohtmlCave, 0, MEM_RELEASE);
        Log("[menus] no free memory within reach of the game and Cohtml, nothing patched");
        return;
    }
    g_menuShown = gameCave + page;

    BYTE* rotationStub = gameCave;
    memcpy(rotationStub, kRotationStub, sizeof kRotationStub);
    int32_t flagDisp = (int32_t)(g_menuShown - (rotationStub + kRotationStubFlagNextIp));
    memcpy(rotationStub + kRotationStubFlagDispAt, &flagDisp, 4);
    BYTE* everyView = game + kRvaEveryView;
    BYTE* afterPick = game + kRvaAfterRotationPick;
    memcpy(rotationStub + kRotationStubEveryViewAt, &everyView, 8);
    memcpy(rotationStub + kRotationStubBackAt, &afterPick, 8);

    BYTE* urlStub = cohtmlCave;
    memcpy(urlStub, kUrlStub, sizeof kUrlStub);
    void* hook = (void*)&OnLoadUrl;
    BYTE* urlBack = cohtml + kLoadUrl.rva + 5;
    memcpy(urlStub + kUrlStubHookAt, &hook, 8);
    memcpy(urlStub + kUrlStubBackAt, &urlBack, 8);

    BYTE pickJump[5], urlJump[5];
    DWORD old = 0;
    if (!EncodeJump(game + kRvaRotationPick, rotationStub, pickJump) || !EncodeJump(cohtml + kLoadUrl.rva, urlStub, urlJump) ||
        !VirtualProtect(gameCave, page, PAGE_EXECUTE_READ, &old) || !VirtualProtect(cohtmlCave, page, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(gameCave, 0, MEM_RELEASE);
        VirtualFree(cohtmlCave, 0, MEM_RELEASE);
        g_menuShown = nullptr;
        Log("[menus] the stubs could not be placed within reach, nothing patched");
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), gameCave, page);
    FlushInstructionCache(GetCurrentProcess(), cohtmlCave, page);

    // The URL hook goes in first, so the rotation stub never reads a state nothing has set yet. Until a
    // page loads it reads 0 and the game rotates as it always did.
    if (!WriteCode(cohtml + kLoadUrl.rva, urlJump, sizeof urlJump)) {
        Log("[menus] could not patch Cohtml's URL loader, nothing patched");
        return;
    }
    if (!WriteCode(game + kRvaRotationPick, pickJump, sizeof pickJump)) {
        Log("[menus] could not patch the UI surface rotation, menus update as the game does");
        return;
    }
    Log("[menus] menu refresh fix on, a menu page in a session updates every frame, the HUD and the car displays keep their turns");
}
