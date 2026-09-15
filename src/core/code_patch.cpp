#include "acevo/core/code_patch.h"

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

bool WriteCode(BYTE* at, const BYTE* code, size_t length)
{
    DWORD old = 0;
    if (!VirtualProtect(at, length, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(at, code, length);
    DWORD ignored = 0;
    VirtualProtect(at, length, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), at, length);
    return true;
}
