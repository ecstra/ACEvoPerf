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
//   - The count moves from one to zero only by compare and exchange, so a std::weak_ptr lock elsewhere cannot
//     revive it after that.
//   - What it rests on is that only the game thread touches a finished session. The free runs only on that
//     thread, inside the connect, so a plain copy of the list's entry, which raises the count without looking,
//     or a push that moves the list between the entry being found and cleared, would have to come from another
//     thread at that moment. It also rests on the manager still holding the session before last at each
//     connect, which is why every free comes a whole session after the freed one ended. Every free logged so
//     far ran on GameThread that late, with no fault in any game log kept from those runs, but the game's code
//     that reads the list was not examined. Waiting a connect before the destroy would take away the copy,
//     though not the push, at the cost of a finished session held through a load, and DEC-023 records why
//     it does not.
//   - A connection whose game mode is null, whose list does not hold it, or that anything else still holds is
//     left alone. A connection kept past its game mode is one of those while the block still holds that game
//     mode, since its destructor emptied the list, see EntryFor, which also says what happens once another
//     object takes the block. One whose game mode can no longer be read, or no longer starts with a vtable of
//     the exe, is let go for good, since it can never be freed without deleting that game mode a second time.
//     Every read of the game mode and its list is checked readable before it is made, and fault guarded as
//     well.

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
static std::atomic<uint32_t> g_freed{0};
// The thread the mod was loaded on, the process's first, which the game names GameThread and runs its main
// loop and every connect on. The free runs only there, see HookMakeConnection.
static DWORD g_gameThread = 0;

// Holds g_lock for a scope. A push_back can throw only when memory has run out, and a throw that left the
// lock held would hang the next connect in AcquireSRWLockExclusive.
struct ExclusiveLock {
    ExclusiveLock() { AcquireSRWLockExclusive(&g_lock); }
    ~ExclusiveLock() { ReleaseSRWLockExclusive(&g_lock); }
    ExclusiveLock(const ExclusiveLock&) = delete;
    ExclusiveLock& operator=(const ExclusiveLock&) = delete;
};

static ControlBlockFn Slot(BYTE* control, int slot)
{
    return (ControlBlockFn)At<void* const*>(control, 0)[slot];
}

static void ReleaseWeak(BYTE* control)
{
    if (_InterlockedDecrement((long*)(control + control::kWeaks)) == 0)
        Slot(control, control::kSlotDeleteThis)(control);
}

static bool InImage(const void* p, size_t length)
{
    const BYTE* at = (const BYTE*)p;
    return at >= g_exe && at <= g_exe + kSizeOfImage && length <= (size_t)(g_exe + kSizeOfImage - at);
}

// True when all of [p, p + length) is committed memory that can be read. Asked before each read of a game
// mode's memory, which the heap may have given back since, because the game's crash handler logs every fault
// as a crash, naming the mod when the read is the mod's, even for one a __try then handles, BUG-022, and
// stalls the thread 120 to 210 ms while it writes it, see the game log section of the telemetry doc.
static bool Readable(const void* p, size_t length)
{
    const uintptr_t start = (uintptr_t)p;
    if (length > UINTPTR_MAX - start) return false;

    for (uintptr_t at = start; at < start + length;) {
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery((const void*)at, &info, sizeof info)) return false;
        const DWORD access = info.Protect & 0xFF;
        const bool canRead = access == PAGE_READONLY || access == PAGE_READWRITE || access == PAGE_WRITECOPY ||
                             access == PAGE_EXECUTE_READ || access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY;
        if (info.State != MEM_COMMIT || !canRead || (info.Protect & PAGE_GUARD)) return false;
        at = (uintptr_t)info.BaseAddress + info.RegionSize;
    }
    return true;
}

// The class of a game mode from its RTTI, ".?AVTimeAttackRemote@@" read as TimeAttackRemote. False when its
// first word cannot be read or is not a vtable in the exe's image whose locator and type name are in the
// image too, which is how a game mode shows once the heap has given its memory back or written its own links
// over the start of it, or the block went to something that is not one of the exe's objects. One the game
// destroyed with nothing written over it since still passes, with the vtable of the last base class its
// destructor reached, and so does a block another of the exe's objects has taken, with that object's class,
// see EntryFor. The game mode's word is checked readable before it is read, the rest lies in the exe's
// image, and a fault on the way is still caught, since the pointer comes out of memory the game owns.
static bool GameModeClass(const BYTE* gameMode, char* out, size_t size)
{
    strcpy_s(out, size, "unknown");
    if (!Readable(gameMode, sizeof(void*))) return false;
    __try {
        const void* const* vtable = At<const void* const*>(gameMode, 0);
        if (!InImage(vtable - 1, 2 * sizeof(void*))) return false;
        const BYTE* locator = (const BYTE*)vtable[-1];
        if (!InImage(locator, 16)) return false;
        const char* name = (const char*)(g_exe + At<int32_t>(locator, 12) + 0x10);
        // A class's type name starts .?AV and a struct's .?AU.
        if (!InImage(name, 5) || (strncmp(name, ".?AV", 4) != 0 && strncmp(name, ".?AU", 4) != 0)) return false;
        size_t length = strcspn(name + 4, "@");
        if (length >= size) length = size - 1;
        memcpy(out, name + 4, length);
        out[length] = 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        strcpy_s(out, size, "unknown");
        return false;
    }
}

// The entry of the game mode's connection list that holds this connection, or null. A connection kept past
// its game mode, which a session restarted from the pause menu might leave, points at freed memory, and
// freeing it would delete that game mode a second time. While the block still holds the destroyed game mode
// the walk finds nothing, even though the block still passes GameModeClass, because the game mode's
// destructor released the list and MSVC's vector leaves its pointers null when it goes. That rests on the
// vector as MSVC builds it, and the exe's copy was not checked for that. Once another of the exe's objects
// takes the block, the walk reads that object's words as a list, where a match would need this very
// connection's control block where an entry's sits, unlikely but not ruled out. `gameModeDead` says the
// memory can no longer be read or no longer holds one of the exe's objects at all, each read checked before
// it is made, see Readable.
static BYTE* EntryFor(BYTE* control, bool& gameModeDead)
{
    BYTE* gameMode = At<BYTE*>(control, control::kGameMode);
    if (!gameMode) return nullptr;

    char name[64];
    if (!GameModeClass(gameMode, name, sizeof name) || !Readable(gameMode + gamemode::kConnectionsFirst, 3 * sizeof(BYTE*))) {
        gameModeDead = true;
        return nullptr;
    }

    __try {
        BYTE* first = At<BYTE*>(gameMode, gamemode::kConnectionsFirst);
        BYTE* last = At<BYTE*>(gameMode, gamemode::kConnectionsLast);
        BYTE* end = At<BYTE*>(gameMode, gamemode::kConnectionsEnd);
        if (!first || last < first || end < last) return nullptr;
        if ((last - first) % gamemode::kEntrySize || (size_t)(last - first) > gamemode::kMaxEntries * gamemode::kEntrySize) return nullptr;
        if (!Readable(first, (size_t)(last - first))) {
            gameModeDead = true;
            return nullptr;
        }

        for (BYTE* entry = first; entry < last; entry += gamemode::kEntrySize) {
            if (At<BYTE*>(entry, gamemode::kEntryControl) == control) return entry;
        }
        return nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        gameModeDead = true;
        return nullptr;
    }
}

// The game mode's class for the log, read the same guarded way.
static void GameModeName(BYTE* control, char* out, size_t size)
{
    GameModeClass(At<BYTE*>(control, control::kGameMode), out, size);
}

// Ends the one strong reference the cycle holds. False when the count is no longer one.
static bool Free(BYTE* control)
{
    bool gameModeDead = false;
    BYTE* entry = EntryFor(control, gameModeDead);
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
    uint32_t letGo = 0;
    {
        ExclusiveLock lock;
        // Reserved before anything moves, so a push below cannot throw after earlier connections have been
        // taken out of g_connections, which would lose them with this vector.
        finished.reserve(g_connections.size());
        for (auto it = g_connections.begin(); it != g_connections.end();) {
            BYTE* control = *it;
            long uses = At<long>(control, control::kUses);
            if (uses == 0) {
                // The game freed this one by itself.
                ReleaseWeak(control);
                it = g_connections.erase(it);
                continue;
            }
            bool gameModeDead = false;
            if (uses == 1 && EntryFor(control, gameModeDead)) {
                finished.push_back(control);
                it = g_connections.erase(it);
                continue;
            }
            if (gameModeDead) {
                // Its game mode does not look live, so the connection is let go for good and stays in memory
                // as it would without the fix, see EntryFor.
                ReleaseWeak(control);
                it = g_connections.erase(it);
                ++letGo;
                continue;
            }
            ++it;
        }
    }
    if (letGo) {
        Log("[sessions] let go of %u connection(s) for good, their game mode does not look live any more, so they "
            "stay in memory as they would without the fix", letGo);
    }

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
            ExclusiveLock lock;
            g_connections.push_back(control);
            continue;
        }
        const uint32_t freedSoFar = ++g_freed;
        Log("[sessions] freed a finished %s session the game kept in memory, %.1f ms, %u freed so far",
            name, (stopped.QuadPart - started.QuadPart) * 1000.0 / frequency.QuadPart, freedSoFar);
    }
}

// Returns how many connections are tracked with this one.
static size_t Track(BYTE* control)
{
    ExclusiveLock lock;
    if (control && At<const void*>(control, 0) == g_controlBlockVtable) {
        // Pushed before the weak reference is taken, so a push that throws leaves nothing untracked.
        g_connections.push_back(control);
        _InterlockedIncrement((long*)(control + control::kWeaks));
    }
    return g_connections.size();
}

// Replaces the one call of the connection's make_shared. The finished sessions are freed once the new
// connection exists, when everything the connect read has been used.
static void* HookMakeConnection(BYTE* out, void* a2, void* a3, void* a4, void* a5, void* a6)
{
    void* result = g_makeConnection(out, a2, a3, a4, a5, a6);

    // The free rests on only the game thread touching a finished session, which holds only while the free
    // runs on that thread too, and on the manager still holding the session before last, see the comment at
    // the top of this file. A connect from any other thread still tracks its connection, and the sessions it
    // would have freed wait for the next connect on the game thread.
    const bool onGameThread = GetCurrentThreadId() == g_gameThread;
    if (onGameThread) FreeFinishedSessions();
    size_t tracked = Track(At<BYTE*>(out, 8));

    // Which thread connects decides what else could still be touching a freed session, so the log names it.
    wchar_t* thread = nullptr;
    bool named = SUCCEEDED(GetThreadDescription(GetCurrentThread(), &thread)) && thread && thread[0];
    Log("[sessions] a session connected on thread '%ls' (%lu), %zu connections held with it, %u freed so far%s",
        named ? thread : L"unnamed", GetCurrentThreadId(), tracked, g_freed.load(),
        onGameThread ? "" : ", not the game thread, so nothing was freed");
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
    // DllMain of a DLL the exe imports runs on the process's first thread, the one the game names GameThread.
    g_gameThread = GetCurrentThreadId();
    if (!WriteCode(base + kRvaMakeConnectionCall, call, sizeof call)) {
        Log("[sessions] could not patch the exe at rva 0x%07X, nothing patched", (unsigned)kRvaMakeConnectionCall);
        return;
    }
    Log("[sessions] session leak fix on, a finished session the game keeps in memory is freed when a later one connects "
        "on the game thread (%lu)", g_gameThread);
}
