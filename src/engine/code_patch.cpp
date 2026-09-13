#include "acevo/engine/code_patch.h"

uint64_t Fnv1a64(const BYTE* p, size_t n)
{
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

BYTE* AllocNear(BYTE* anchor, size_t size)
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
