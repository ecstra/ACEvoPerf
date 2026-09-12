// The engine's spin lock, see include/acevo/engine/job_lock.h.
//
// A session load spends about a quarter of a busy Resource Manager Worker's time in one spin
// loop, and at rest the same thread spends 0.8 percent there, so it is the load that creates it
// (TODO-013). The engine's own loading boost takes the pool from 2 to 11 workers, which is what
// puts eleven threads on the loop at once.
//
// The loop itself is the reason that hurts. It is a bare test and set:
//
//     pause
//     mov  ecx, eax            ; eax holds the value to store, it never changes in the loop
//     xchg dword ptr [rbx], ecx ; a locked write, every single iteration
//     test ecx, ecx
//     jne  back                 ; ten bytes in total
//
// Every waiting thread issues a locked write each time round, so each one takes the cache line
// exclusively and steals it from the thread actually holding the lock, which is the one thread
// that needs to make progress. The fix is the textbook one, test and test and set: read the lock
// with a plain load until it looks free, and only then try the exchange. Waiting threads then sit
// on a shared copy of the line and leave the owner alone. It does not change what the lock does,
// only how loudly it waits, and the exchange that actually takes the lock is untouched.
//
// Why this is safe to do by overwriting those bytes:
//
//   - The ten bytes pin their own meaning. Any code matching them is a test and set spin on the
//     dword at rbx with the value in eax, so the replacement is correct for every site that
//     matches, without knowing anything about the surrounding function.
//   - eax and rbx are only read inside the loop, never written, so they are the same on every
//     iteration and the replacement may read them freely.
//   - The replacement clobbers ecx and the flags, exactly what the original clobbered.
//   - Nothing branches into the middle. Every jump, call and loop instruction in the exe's .text
//     was disassembled, 23.4 million of them, and the only branch landing inside any matching
//     loop is that loop's own back edge.
//   - It is applied from DllMain, before the game's entry point runs, so no other thread exists
//     and no thread can be inside the bytes being replaced.
#include "acevo/engine/job_lock.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"

// pause / mov ecx,eax / xchg [rbx],ecx / test ecx,ecx / jne -10
static const BYTE kSpinLoop[] = { 0xF3, 0x90, 0x8B, 0xC8, 0x87, 0x0B, 0x85, 0xC9, 0x75, 0xF6 };
static const size_t kSpinLen = sizeof kSpinLoop;

// A run of these in one exe would mean the pattern is not what this code thinks it is, so it
// refuses rather than rewriting the world.
static const int kMaxSites = 8;

// pause / mov ecx,[rbx] / test / jne self / mov ecx,eax / xchg / test / jne self / jmp back
static const BYTE kReplacement[] = {
    0xF3, 0x90,             // pause
    0x8B, 0x0B,             // mov  ecx, dword ptr [rbx]   a plain read, no bus lock
    0x85, 0xC9,             // test ecx, ecx
    0x75, 0xF8,             // jne  -8, back to the pause, while the lock looks taken
    0x8B, 0xC8,             // mov  ecx, eax
    0x87, 0x0B,             // xchg dword ptr [rbx], ecx   the one locked write
    0x85, 0xC9,             // test ecx, ecx
    0x75, 0xF0,             // jne  -16, someone got in first, go back to reading
    0xE9, 0, 0, 0, 0,       // jmp  rel32 to the instruction after the original loop
};
static const size_t kReplacementLen = sizeof kReplacement;
// Index of the 0xE9 itself. Its displacement follows at +1 and the next instruction is at +5.
static const size_t kJmpRel32Offset = 16;
static_assert(sizeof kReplacement == 21, "the replacement is 21 bytes, the jump starts at 16");

static BYTE* g_caveLo = nullptr;
static BYTE* g_caveHi = nullptr;

void JobLockCave(const BYTE** lo, const BYTE** hi)
{
    *lo = g_caveLo;
    *hi = g_caveHi;
}

// A 32 bit displacement reaches 2 GB either way, so the replacement has to live near the exe.
static BYTE* AllocCaveNear(BYTE* anchor, size_t size)
{
    SYSTEM_INFO si = {};
    GetSystemInfo(&si);
    const uintptr_t granularity = si.dwAllocationGranularity ? si.dwAllocationGranularity : 0x10000;
    const uintptr_t reach = 0x60000000ull;   // well inside 2 GB, leaving room for the exe itself

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

static bool WriteCode(BYTE* at, const BYTE* bytes, size_t len)
{
    DWORD old = 0;
    if (!VirtualProtect(at, len, PAGE_READWRITE, &old)) return false;
    memcpy(at, bytes, len);
    DWORD ignored = 0;
    VirtualProtect(at, len, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), at, len);
    return true;
}

void PatchJobQueueSpinLock()
{
    if (!g_cfg.jobLockFix) return;

    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    auto dos = (IMAGE_DOS_HEADER*)base;
    auto nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    BYTE* textLo = nullptr;
    BYTE* textHi = nullptr;
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (memcmp(sec[i].Name, ".text", 6) != 0) continue;
        textLo = base + sec[i].VirtualAddress;
        textHi = textLo + sec[i].Misc.VirtualSize;
        break;
    }
    if (!textLo) { Log("[joblock] no .text section in the exe, nothing patched"); return; }

    BYTE* sites[kMaxSites] = {};
    int found = 0;
    for (BYTE* p = textLo; p <= textHi - kSpinLen; ++p) {
        if (p[0] != kSpinLoop[0] || memcmp(p, kSpinLoop, kSpinLen) != 0) continue;
        if (found < kMaxSites) sites[found] = p;
        ++found;
    }

    if (!found) {
        Log("[joblock] the spin loop pattern is not in this build, nothing patched. The game runs "
            "exactly as it would without the mod's fix.");
        return;
    }
    if (found > kMaxSites) {
        Log("[joblock] %d matches for the spin loop pattern, more than the %d this expects. That "
            "means the pattern no longer means what it did, so nothing is patched.", found, kMaxSites);
        return;
    }

    BYTE* cave = AllocCaveNear(textLo, kReplacementLen * found);
    if (!cave) { Log("[joblock] no free page within reach of the exe, nothing patched"); return; }

    int patched = 0;
    for (int i = 0; i < found; ++i) {
        BYTE* site = sites[i];
        BYTE* trampoline = cave + (size_t)patched * kReplacementLen;
        BYTE* afterLoop = site + kSpinLen;

        // The replacement first, so the jump never points at a page that is not ready yet.
        memcpy(trampoline, kReplacement, kReplacementLen);
        const int64_t backDelta = (int64_t)(afterLoop - (trampoline + kJmpRel32Offset + 5));
        const int64_t jumpDelta = (int64_t)(trampoline - (site + 5));
        if (backDelta > INT32_MAX || backDelta < INT32_MIN || jumpDelta > INT32_MAX || jumpDelta < INT32_MIN) {
            Log("[joblock] rva 0x%08X is out of reach of the replacement, left alone",
                (unsigned)(site - base));
            continue;
        }
        *(int32_t*)(trampoline + kJmpRel32Offset + 1) = (int32_t)backDelta;

        // int3 after the jump rather than nop: nothing can reach those five bytes, and if that
        // were ever wrong a loud stop is better than falling into the lock acquired path without
        // holding the lock.
        BYTE detour[kSpinLen];
        memset(detour, 0xCC, sizeof detour);
        detour[0] = 0xE9;
        *(int32_t*)(detour + 1) = (int32_t)jumpDelta;

        if (memcmp(site, kSpinLoop, kSpinLen) != 0) continue;   // changed under us, leave it
        if (!WriteCode(site, detour, kSpinLen)) {
            Log("[joblock] could not make rva 0x%08X writable, left alone", (unsigned)(site - base));
            continue;
        }
        if (memcmp(site, detour, kSpinLen) != 0) {
            Log("[joblock] rva 0x%08X did not take the write, left alone", (unsigned)(site - base));
            continue;
        }

        Log("[joblock] rva 0x%08X patched, the spin now reads before it writes", (unsigned)(site - base));
        ++patched;
    }

    if (!patched) { VirtualFree(cave, 0, MEM_RELEASE); Log("[joblock] nothing patched"); return; }

    DWORD old = 0;
    VirtualProtect(cave, kReplacementLen * patched, PAGE_EXECUTE_READ, &old);
    FlushInstructionCache(GetCurrentProcess(), cave, kReplacementLen * patched);
    g_caveLo = cave;
    g_caveHi = cave + kReplacementLen * patched;

    Log("[joblock] %d of %d spin loops now read before they write. The engine's job queue lock is "
        "the one that matters, a load had eleven workers on it burning a quarter of their time.",
        patched, found);
}
