#include "acevo/ui/cohtml_hooks.h"
#include "acevo/core/iat.h"
#include "acevo/core/log.h"

// Read from AssettoCorsaEVO.exe 0.9.1 (the Steam build of 2026-09-11) and cohtml.WindowsDesktop.dll
// 1.61.0.3. The game's UI object (EvoUi) keeps its GameUi behind [this+0xA8]. Its slot 7 posts the frame's
// UI job (every listed view's Advance and the paint) with the UI clock as a float in xmm1 and a pointer in
// r8. Its slot 8 waits for that job while running other queued jobs and passes the renderer in rdx. Both
// wrappers pass every argument on.

static const DWORD kTimeDateStamp = 0x6A9EC72A;
static const DWORD kSizeOfImage = 0x06CDD000;
static const uint32_t kRvaEvoUiVtable = 0x3173D58;
static const int kPostFrameSlot = 7;
static const int kEndFrameSlot = 8;
static const uint32_t kRvaPostFrameThunk = 0x0126E58;
static const uint32_t kRvaEndFrameThunk = 0x00EB461;

// The slot numbers in cohtml_slot belong to the UI engine build they were read from, and Kunos ships
// Coherent Gameface as its own binary that can move without the exe changing, so the exe stamp above does
// not cover them. A slot that moved is an indirect call through the wrong method with the wrong signature,
// which is a crash before a menu is ever drawn, so a build that is not this one refuses rather than tries.
// The stamp and the image size are the test, the version name is for the log line. Both constants are the
// ones the five byte patch files already require, so a build that passes here is one they can patch.
static const DWORD kCohtmlTimeDateStamp = 0x675439C7;
static const DWORD kCohtmlSizeOfImage = 0x0070F000;
static const char* kCohtmlVersionName = "1.61.0.3";

typedef void* (*PFN_LibraryInitialize)(const char* licenseKey, const void* params);
typedef void* (*PFN_CreateSystem)(void* library, const void* settings);
typedef void* (*PFN_CreateView)(void* system, const void* settings);
typedef void (*PFN_PostFrame)(void* evoUi, float uiClock, void* arg3, void* arg4);
typedef void (*PFN_EndFrame)(void* evoUi, void* renderer, void* arg3, void* arg4);

static const int kMaxListeners = 4;

static CohtmlLibraryListener g_libraryListeners[kMaxListeners];
static CohtmlViewListener g_viewListeners[kMaxListeners];
static UiFramePostListener g_postListeners[kMaxListeners];
static UiFrameEndListener g_endListeners[kMaxListeners];
static int g_libraryListenerCount = 0;
static int g_viewListenerCount = 0;
static int g_postListenerCount = 0;
static int g_endListenerCount = 0;
static bool g_installed = false;

static PFN_LibraryInitialize g_origInitialize = nullptr;
static PFN_CreateSystem g_origCreateSystem = nullptr;
static PFN_CreateView g_origCreateView = nullptr;
static PFN_PostFrame g_origPostFrame = nullptr;
static PFN_EndFrame g_origEndFrame = nullptr;
static std::atomic<int> g_viewsCreated{0};

void AddCohtmlLibraryListener(CohtmlLibraryListener listener)
{
    if (g_libraryListenerCount < kMaxListeners) g_libraryListeners[g_libraryListenerCount++] = listener;
}

void AddCohtmlViewListener(CohtmlViewListener listener)
{
    if (g_viewListenerCount < kMaxListeners) g_viewListeners[g_viewListenerCount++] = listener;
}

void AddUiFramePostListener(UiFramePostListener listener)
{
    if (g_postListenerCount < kMaxListeners) g_postListeners[g_postListenerCount++] = listener;
}

void AddUiFrameEndListener(UiFrameEndListener listener)
{
    if (g_endListenerCount < kMaxListeners) g_endListeners[g_endListenerCount++] = listener;
}

// ViewSettings keeps the width at +0x10 and the height at +0x14. The first view is the menu and HUD
// view, the car displays come after it and are created again at every session load.
static void* Hook_CreateView(void* system, const void* settings)
{
    // Read before the original runs, so that a view the engine refuses can still be named in the log. That
    // puts it ahead of the engine's own argument checking, which is the one read in this file that nothing
    // has validated, so a null the engine would have rejected is tolerated here instead of faulted on.
    unsigned width = settings ? *(const unsigned*)((const BYTE*)settings + 0x10) : 0;
    unsigned height = settings ? *(const unsigned*)((const BYTE*)settings + 0x14) : 0;
    void* view = g_origCreateView(system, settings);
    if (!view) {
        Log("[cohtml] view %ux%u was not created", width, height);
        return view;
    }
    int number = ++g_viewsCreated;
    Log("[cohtml] view #%d %ux%u created", number, width, height);
    for (int i = 0; i < g_viewListenerCount; ++i) g_viewListeners[i](view, number, width, height);
    return view;
}

static void* Hook_CreateSystem(void* library, const void* settings)
{
    void* system = g_origCreateSystem(library, settings);
    if (!system) {
        Log("[cohtml] system was not created");
        return system;
    }
    HookVtableSlot(*(void***)system, cohtml_slot::kSystemCreateView, (void*)&Hook_CreateView, (void**)&g_origCreateView, "Cohtml System::CreateView");
    return system;
}

static void* Hook_LibraryInitialize(const char* licenseKey, const void* params)
{
    void* library = g_origInitialize(licenseKey, params);
    if (!library) {
        Log("[cohtml] library initialisation failed");
        return library;
    }
    HookVtableSlot(*(void***)library, cohtml_slot::kLibraryCreateSystem, (void*)&Hook_CreateSystem, (void**)&g_origCreateSystem, "Cohtml Library::CreateSystem");
    for (int i = 0; i < g_libraryListenerCount; ++i) g_libraryListeners[i](library);
    return library;
}

static void Hook_PostFrame(void* evoUi, float uiClock, void* arg3, void* arg4)
{
    for (int i = 0; i < g_postListenerCount; ++i) g_postListeners[i](uiClock);
    g_origPostFrame(evoUi, uiClock, arg3, arg4);
}

static void Hook_EndFrame(void* evoUi, void* renderer, void* arg3, void* arg4)
{
    for (int i = 0; i < g_endListenerCount; ++i) g_endListeners[i](false);
    g_origEndFrame(evoUi, renderer, arg3, arg4);
    for (int i = g_endListenerCount - 1; i >= 0; --i) g_endListeners[i](true);
}

// A loaded module's file version, read out of its own version resource and left at zero if it carries
// none. Only the log line reads this, the stamp is what decides anything, so a module with no version
// resource is not a failure here. kernel32 only, so it needs no import the proxy does not already carry.
static void ModuleFileVersion(HMODULE module, uint16_t out[4])
{
    HRSRC found = FindResourceW(module, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(16));
    if (!found) return;
    DWORD size = SizeofResource(module, found);
    HGLOBAL loaded = LoadResource(module, found);
    const BYTE* data = loaded ? (const BYTE*)LockResource(loaded) : nullptr;
    if (!data) return;

    // VS_VERSIONINFO puts a variable length key and its alignment padding in front of the fixed block, so
    // find that block by its own signature rather than walking the layout. The four dwords from there are
    // dwSignature, dwStrucVersion, dwFileVersionMS and dwFileVersionLS.
    for (DWORD at = 0; at + 16 <= size; at += 4) {
        const uint32_t* fixed = (const uint32_t*)(data + at);
        if (fixed[0] != 0xFEEF04BD) continue;
        out[0] = (uint16_t)(fixed[2] >> 16);
        out[1] = (uint16_t)(fixed[2] & 0xFFFF);
        out[2] = (uint16_t)(fixed[3] >> 16);
        out[3] = (uint16_t)(fixed[3] & 0xFFFF);
        return;
    }
}

// Whatever is loaded under the UI engine's name is untrusted until it has been read. A packed, wrapped or
// simply different build can carry a header this does not expect or claim a resource size past the end of
// its own image, and this is the one read whose whole job is to survive a build the mod does not know, so
// it refuses on a fault rather than taking one.
static bool ReadModuleBuild(HMODULE module, DWORD* stamp, DWORD* image, uint16_t version[4])
{
    __try {
        auto dos = (IMAGE_DOS_HEADER*)module;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        auto nt = (IMAGE_NT_HEADERS64*)((BYTE*)module + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
        *stamp = nt->FileHeader.TimeDateStamp;
        *image = nt->OptionalHeader.SizeOfImage;
        ModuleFileVersion(module, version);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static void InstallUiFrameHooks()
{
    if (!g_postListenerCount && !g_endListenerCount) return;

    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    auto nt = (IMAGE_NT_HEADERS64*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != kTimeDateStamp || nt->OptionalHeader.SizeOfImage != kSizeOfImage) {
        Log("[cohtml] this is not the game build the UI frame hooks were written for, the UI frame is not followed");
        return;
    }
    void** vtable = (void**)(base + kRvaEvoUiVtable);
    if ((BYTE*)vtable[kPostFrameSlot] != base + kRvaPostFrameThunk || (BYTE*)vtable[kEndFrameSlot] != base + kRvaEndFrameThunk) {
        Log("[cohtml] the game UI's vtable is not where it was read, the UI frame is not followed");
        return;
    }
    if (g_postListenerCount) HookVtableSlot(vtable, kPostFrameSlot, (void*)&Hook_PostFrame, (void**)&g_origPostFrame, "the game UI frame post");
    if (g_endListenerCount) HookVtableSlot(vtable, kEndFrameSlot, (void*)&Hook_EndFrame, (void**)&g_origEndFrame, "the game UI frame end");
}

bool UiFrameEndHooked()
{
    return g_origEndFrame != nullptr;
}

void InstallCohtmlHooks()
{
    if (g_installed) return;
    g_installed = true;
    if (!g_libraryListenerCount && !g_viewListenerCount && !g_postListenerCount && !g_endListenerCount) return;

    InstallUiFrameHooks();

    HMODULE cohtml = GetModuleHandleW(L"cohtml.WindowsDesktop.dll");
    void* initialize = cohtml ? (void*)GetProcAddress(cohtml, "?Initialize@Library@cohtml@@SAPEAV12@PEBDAEBULibraryParams@2@@Z") : nullptr;
    if (!initialize) {
        Log("[cohtml] Library::Initialize not found, nothing that needs the UI engine's objects runs");
        return;
    }
    DWORD stamp = 0, image = 0;
    uint16_t version[4] = {};
    bool read = ReadModuleBuild(cohtml, &stamp, &image, version);
    if (!read || stamp != kCohtmlTimeDateStamp || image != kCohtmlSizeOfImage) {
        Log("[cohtml] the UI engine here is %u.%u.%u.%u (stamp 0x%08X image 0x%08X) and the objects were read from %s (stamp 0x%08X image 0x%08X), nothing that uses its vtables runs",
            (unsigned)version[0], (unsigned)version[1], (unsigned)version[2], (unsigned)version[3],
            stamp, image, kCohtmlVersionName, kCohtmlTimeDateStamp, kCohtmlSizeOfImage);
        return;
    }
    Log("[cohtml] UI engine %u.%u.%u.%u (stamp 0x%08X), the build its objects were read from",
        (unsigned)version[0], (unsigned)version[1], (unsigned)version[2], (unsigned)version[3], stamp);
    g_origInitialize = (PFN_LibraryInitialize)initialize;
    int patched = PatchIatByAddress(GetModuleHandleW(nullptr), initialize, (void*)&Hook_LibraryInitialize);
    Log("[cohtml] Library::Initialize, %d import slot(s) of the exe patched", patched);
}
