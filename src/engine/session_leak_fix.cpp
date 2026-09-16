// The sessions the game never frees, see include/acevo/engine/session_leak_fix.h.
//
// What goes wrong. Every session, a practice at a track or the menu, runs on a local server connection that
// the game makes with std::make_shared (0x1245B00, from its one caller at 0x124AAE8). The connection embeds a
// LocalGameServer (+0x4B8) that owns the session's game mode (+0x160, TimeAttackRemote for a practice,
// PaintShopGameMode for the menu), and the game mode's base, RemoteGameMode, keeps its connections in a
// std::vector of std::shared_ptr (+0x3E8, released by 0x1921B10 in its destructor). So the game mode holds a
// strong reference to the connection that owns it. When a session ends, GameServerConnectionManager lets go
// of the connection, the cycle keeps the count at one, and the connection, its game mode, the session's
// state, the track's parsed scene three times over, the weather services and a physics body stay in memory
// until the process ends. At the Red Bull Ring that is about 57 MB a visit (the census run of 2026-09-16,
// session-leak-census-2026-09-16).
//
// The fix. The call that makes each connection is hooked. After the game has made a new one, every
// connection made before it is looked at, and one whose only strong reference left is the entry in its own
// game mode's list is finished, since nothing outside the cycle can reach it any more. Its count is taken
// from one to zero, the entry cleared so the list's destructor skips it, and the control block's own destroy
// and delete run, the same calls a last std::shared_ptr going away makes. A session the manager still holds
// when the next one connects waits for the connect after that.
//
// Why this is safe to do:
//
//   - Every region the patch relies on is compared with this build byte for byte before anything is written,
//     and none of those ranges carries a base relocation.
//   - The hook keeps a weak reference of its own on each connection, the way a std::weak_ptr does, so a
//     control block the game frees by itself stays readable until the hook lets go of it.
//   - The count moves from one to zero only by compare and exchange. A std::weak_ptr lock elsewhere cannot
//     revive it after that, and a strong copy could only come from the entry being cleared.
//   - A connection whose game mode is gone, whose list does not hold it, or that anything else still holds is
//     left alone.

#include "acevo/engine/session_leak_fix.h"
#include "acevo/core/code_patch.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"

// AssettoCorsaEVO.exe 0.9.1, the Steam build of 2026-09-11.
static const DWORD kTimeDateStamp = 0x6A9EC72A;
static const DWORD kSizeOfImage = 0x06CDD000;

struct Region {
    uint32_t rva;
    uint32_t length;
    uint64_t fnv1a64;
    const char* what;
};

static const Region kRegions[] = {
    { 0x124AAC1, 0x061, 0xA14FE5658AE23494ull, "the local server connect around the connection's creation" },
    { 0x1245B00, 0x151, 0x89B4DAAD53900BB4ull, "the connection's make_shared" },
    { 0x0007A3B, 0x005, 0x97794F770B3898ACull, "the make_shared thunk" },
    { 0x1950C50, 0x0A0, 0x1D6A3EF12E48DDC1ull, "the connection's destructor" },
    { 0x1B24FA0, 0x04F, 0xEEF1680D0D9BE75Eull, "the local game server's destructor" },
    { 0x19150D1, 0x00C, 0x5F7FB64F4C998A82ull, "the game mode destructor's connection list" },
    { 0x1921B10, 0x080, 0xFBECFFC0998CAC43ull, "the connection list's destructor" },
};

static const uint32_t kRvaMakeConnectionCall = 0x124AAE8;   // call the make_shared thunk, five bytes
static const uint32_t kRvaMakeConnection = 0x1245B00;
static const uint32_t kRvaControlBlockVtable = 0x321BA18;   // std::_Ref_count_obj2<LocalServerConnection>

// The make_shared control block, with the connection built at +0x10.
namespace control {
constexpr ptrdiff_t kUses = 0x08;
constexpr ptrdiff_t kWeaks = 0x0C;
constexpr ptrdiff_t kGameMode = 0x10 + 0x4B8 + 0x160;
constexpr int kSlotDestroy = 0;
constexpr int kSlotDeleteThis = 1;
}

// RemoteGameMode's list of connections, a std::vector of std::shared_ptr.
namespace gamemode {
constexpr ptrdiff_t kConnectionsFirst = 0x3E8;
constexpr ptrdiff_t kConnectionsLast = 0x3F0;
constexpr ptrdiff_t kConnectionsEnd = 0x3F8;
constexpr ptrdiff_t kEntrySize = 16;            // the object pointer, then the control block
constexpr ptrdiff_t kEntryControl = 8;
constexpr size_t kMaxEntries = 64;
}

template <typename T>
static T At(const BYTE* p, ptrdiff_t offset)
{
    T value;
    memcpy(&value, p + offset, sizeof value);
    return value;
}

using MakeConnectionFn = void* (*)(BYTE* out, void* a2, void* a3, void* a4, void* a5, void* a6);
using ControlBlockFn = void (*)(BYTE* control);

static BYTE* g_exe = nullptr;
static const void* g_controlBlockVtable = nullptr;
static MakeConnectionFn g_makeConnection = nullptr;
static SRWLOCK g_lock = SRWLOCK_INIT;
static std::vector<BYTE*> g_connections;     // control blocks the hook holds a weak reference on, under g_lock
static uint32_t g_freed = 0;

static ControlBlockFn Slot(BYTE* control, int slot)
{
    return (ControlBlockFn)At<void* const*>(control, 0)[slot];
}

static void ReleaseWeak(BYTE* control)
{
    if (_InterlockedDecrement((long*)(control + control::kWeaks)) == 0)
        Slot(control, control::kSlotDeleteThis)(control);
}

// The entry of the game mode's connection list that holds this connection, or null.
static BYTE* EntryFor(BYTE* control)
{
    BYTE* gameMode = At<BYTE*>(control, control::kGameMode);
    if (!gameMode) return nullptr;

    BYTE* first = At<BYTE*>(gameMode, gamemode::kConnectionsFirst);
    BYTE* last = At<BYTE*>(gameMode, gamemode::kConnectionsLast);
    BYTE* end = At<BYTE*>(gameMode, gamemode::kConnectionsEnd);
    if (!first || last < first || end < last) return nullptr;
    if ((last - first) % gamemode::kEntrySize || (size_t)(last - first) > gamemode::kMaxEntries * gamemode::kEntrySize) return nullptr;

    for (BYTE* entry = first; entry < last; entry += gamemode::kEntrySize) {
        if (At<BYTE*>(entry, gamemode::kEntryControl) == control) return entry;
    }
    return nullptr;
}

// The game mode's class from its RTTI, ".?AVTimeAttackRemote@@" read as TimeAttackRemote.
static void GameModeName(BYTE* control, char* out, size_t size)
{
    strcpy_s(out, size, "unknown");
    __try {
        BYTE* gameMode = At<BYTE*>(control, control::kGameMode);
        const BYTE* locator = (const BYTE*)At<void* const*>(gameMode, 0)[-1];
        const char* name = (const char*)(g_exe + At<int32_t>(locator, 12) + 0x10);
        if (strncmp(name, ".?AV", 4) != 0) return;
        size_t length = strcspn(name + 4, "@");
        if (length >= size) length = size - 1;
        memcpy(out, name + 4, length);
        out[length] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        strcpy_s(out, size, "unknown");
    }
}

// Ends the one strong reference the cycle holds. False when the count is no longer one.
static bool Free(BYTE* control)
{
    BYTE* entry = EntryFor(control);
    if (!entry) return false;
    if (_InterlockedCompareExchange((long*)(control + control::kUses), 0, 1) != 1) return false;

    memset(entry, 0, gamemode::kEntrySize);
    Slot(control, control::kSlotDestroy)(control);
    ReleaseWeak(control);      // the weak reference every strong count carries
    ReleaseWeak(control);      // the hook's own
    return true;
}

static void FreeFinishedSessions()
{
    std::vector<BYTE*> finished;
    AcquireSRWLockExclusive(&g_lock);
    for (auto it = g_connections.begin(); it != g_connections.end();) {
        BYTE* control = *it;
        long uses = At<long>(control, control::kUses);
        if (uses == 0) {
            // The game freed this one by itself.
            ReleaseWeak(control);
            it = g_connections.erase(it);
            continue;
        }
        if (uses == 1 && EntryFor(control)) {
            finished.push_back(control);
            it = g_connections.erase(it);
            continue;
        }
        ++it;
    }
    ReleaseSRWLockExclusive(&g_lock);

    for (BYTE* control : finished) {
        char name[64];
        GameModeName(control, name, sizeof name);

        LARGE_INTEGER started, stopped, frequency;
        QueryPerformanceCounter(&started);
        bool freed = Free(control);
        QueryPerformanceCounter(&stopped);
        QueryPerformanceFrequency(&frequency);

        if (!freed) {
            // Something took a reference between the look and the free, so it stays tracked.
            AcquireSRWLockExclusive(&g_lock);
            g_connections.push_back(control);
            ReleaseSRWLockExclusive(&g_lock);
            continue;
        }
        g_freed++;
        Log("[sessions] freed a finished %s session the game kept in memory, %.1f ms, %u freed so far",
            name, (stopped.QuadPart - started.QuadPart) * 1000.0 / frequency.QuadPart, g_freed);
    }
}

// Returns how many connections are tracked with this one.
static size_t Track(BYTE* control)
{
    AcquireSRWLockExclusive(&g_lock);
    if (control && At<const void*>(control, 0) == g_controlBlockVtable) {
        _InterlockedIncrement((long*)(control + control::kWeaks));
        g_connections.push_back(control);
    }
    size_t tracked = g_connections.size();
    ReleaseSRWLockExclusive(&g_lock);
    return tracked;
}

// Replaces the one call of the connection's make_shared. The finished sessions are freed once the new
// connection exists, when everything the connect read has been used.
static void* HookMakeConnection(BYTE* out, void* a2, void* a3, void* a4, void* a5, void* a6)
{
    void* result = g_makeConnection(out, a2, a3, a4, a5, a6);
    FreeFinishedSessions();
    size_t tracked = Track(At<BYTE*>(out, 8));

    // Which thread connects decides what else could still be touching a freed session, so the log names it.
    wchar_t* thread = nullptr;
    bool named = SUCCEEDED(GetThreadDescription(GetCurrentThread(), &thread)) && thread && thread[0];
    Log("[sessions] a session connected on thread '%ls' (%lu), %zu connections held with it, %u freed so far",
        named ? thread : L"unnamed", GetCurrentThreadId(), tracked, g_freed);
    if (thread) LocalFree(thread);
    return result;
}

void InstallSessionLeakFix()
{
    if (!g_cfg.sessionLeakFix) return;

    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    auto nt = (IMAGE_NT_HEADERS64*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != kTimeDateStamp || nt->OptionalHeader.SizeOfImage != kSizeOfImage) {
        Log("[sessions] this is not the game build the session leak fix was written for (stamp %08X, image %08X), nothing patched",
            (unsigned)nt->FileHeader.TimeDateStamp, (unsigned)nt->OptionalHeader.SizeOfImage);
        return;
    }
    for (const Region& region : kRegions) {
        if (Fnv1a64(base + region.rva, region.length) != region.fnv1a64) {
            Log("[sessions] %s at rva 0x%07X is not the code this was written against, nothing patched", region.what, (unsigned)region.rva);
            return;
        }
    }

    const size_t page = 0x1000;
    BYTE* cave = AllocNear(base + kRvaMakeConnectionCall, page);
    if (!cave) {
        Log("[sessions] no free memory within reach of the exe, nothing patched");
        return;
    }
    // jmp qword ptr [rip], followed by the absolute target
    const BYTE jump[6] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
    void* hook = (void*)&HookMakeConnection;
    memcpy(cave, jump, sizeof jump);
    memcpy(cave + sizeof jump, &hook, sizeof hook);
    DWORD old = 0;
    if (!VirtualProtect(cave, page, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[sessions] could not make the jump executable, nothing patched");
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), cave, page);

    BYTE call[5];
    if (!EncodeRel32(0xE8, base + kRvaMakeConnectionCall, cave, call)) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[sessions] the hook is out of reach of the exe, nothing patched");
        return;
    }
    g_exe = base;
    g_controlBlockVtable = base + kRvaControlBlockVtable;
    g_makeConnection = (MakeConnectionFn)(base + kRvaMakeConnection);
    if (!WriteCode(base + kRvaMakeConnectionCall, call, sizeof call)) {
        Log("[sessions] could not patch the exe at rva 0x%07X, nothing patched", (unsigned)kRvaMakeConnectionCall);
        return;
    }
    Log("[sessions] session leak fix on, a finished session the game keeps in memory is freed when a later one connects");
}
