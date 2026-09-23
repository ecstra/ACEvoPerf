#include "acevo/core/code_patch.h"
#include "acevo/core/log.h"

uint64_t Fnv1a64(const BYTE* p, size_t n)
{
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

// A 32 bit displacement reaches 2 GB either way.
BYTE* AllocNear(BYTE* anchor, size_t size)
{
    SYSTEM_INFO si = {};
    GetSystemInfo(&si);
    const uintptr_t granularity = si.dwAllocationGranularity ? si.dwAllocationGranularity : 0x10000;
    const uintptr_t reach = 0x60000000ull;   // well inside 2 GB, leaving room for the module itself

    for (uintptr_t delta = granularity; delta < reach; delta += granularity) {
        const uintptr_t base = (uintptr_t)anchor;
        const uintptr_t candidates[2] = { base + delta, base > delta ? base - delta : 0 };
        for (uintptr_t addr : candidates) {
            if (!addr) continue;
            void* p = VirtualAlloc((void*)(addr & ~(granularity - 1)), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (p) return (BYTE*)p;
        }
    }
    return nullptr;
}

bool EncodeRel32(BYTE opcode, const BYTE* from, const BYTE* destination, BYTE* out)
{
    int64_t rel = destination - (from + 5);
    if (rel > INT32_MAX || rel < INT32_MIN) return false;
    int32_t value = (int32_t)rel;
    out[0] = opcode;
    memcpy(out + 1, &value, 4);
    return true;
}

// Set once DllMain is done. Everything that patches code today runs before that, while the game is
// still one thread, which is the only reason a plain copy over live code is safe.
static std::atomic<bool> g_gameIsRunning{false};

void CodePatchingIsNowUnsafe() { g_gameIsRunning.store(true); }
bool CodePatchingIsLate() { return g_gameIsRunning.load(); }

// Copies straight over live code with no thread suspension and no atomic write. A five byte jump
// is five stores, and a thread executing that address mid write runs whatever half is there, which
// is a crash inside the game's own code with nothing of the mod on the stack.
//
// Every caller runs from DLL_PROCESS_ATTACH, before the game has made a second thread, so no
// thread can be there. That is a property of the callers and not of this function, so the function
// says when it has been broken rather than leaving the next caller to find out in someone else's
// game. Making it actually safe means suspending every other thread and checking each one's
// instruction pointer against the range, which is worth writing the day something needs to patch
// late and not before.
//
// Three places copy over code without coming through here, in engine/streamer, ui/restyle_fix and
// ui/ui_probe, because each changes protection over a range in its own shape. They ask
// CodePatchingIsLate for themselves, so this is not the choke point it looks like.
bool WriteCode(BYTE* at, const BYTE* code, size_t length)
{
    if (g_gameIsRunning.load())
        Log("WARNING: code at %p patched after start up, while the game has other threads running. "
            "A thread executing these %zu bytes mid write will crash. See WriteCode.", at, length);

    DWORD old = 0;
    if (!VirtualProtect(at, length, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(at, code, length);
    DWORD ignored = 0;
    VirtualProtect(at, length, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), at, length);
    return true;
}
