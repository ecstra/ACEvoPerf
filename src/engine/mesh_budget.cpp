// The mesh streamer's budget, see include/acevo/engine/mesh_budget.h.
//
// At every scene change the engine sizes two VRAM budgets, the texture tile pool and the mesh
// streamer. force_canonical_pool_sizes, which the mod sets so the tile pool stops shrinking
// (DEC-005), also fixes the mesh budget at the canonical size for the game's texture pool setting,
// 1433 MB on the reference card, where the engine's own sizing gives 366 MB. Measured on 2026-09-13
// that budget costs 2.1 fps parked and 4.7 fps on a lap for a small part of the picture, and no
// flag sets it apart from the tile pool (TODO-022).
//
// One rel32 displacement changes. The budget function returns the canonical size through a jump to
// its shared epilogue, and that jump goes through a stub that loads the configured size into eax
// first. The only caller turns the returned MB into the mesh streamer's byte budget, and the
// dynamic path is left alone.
#include "acevo/engine/mesh_budget.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/engine/code_patch.h"

static const CodeRegion kRegions[] = {
    { 0x1C80CC0, 0x188, 0x8FDCFCDE52CB2BD5ull, "the mesh streamer budget" },
    { 0x0049DD7, 0x005, 0x101FC04EDDF1101Eull, "the mesh budget thunk" },
    { 0x1CCF040, 0x07B, 0xEFC1AB8AF3235120ull, "the scene sizing that applies it" },
};

// jmp to the epilogue right after mov eax, [the canonical size]
static const CodeSite kSiteCanonical = { "the canonical return", 0x1C80D66, 1, 5, 0x1C80E33 };

// The floor the engine's own dynamic path puts under the mesh budget.
static const int kMinMeshBudgetMb = 256;

void InstallMeshBudget()
{
    if (g_cfg.meshBudgetMb == 0) return;
    if (g_cfg.meshBudgetMb < kMinMeshBudgetMb) {
        Log("[mesh] mesh_budget_mb=%d is under the engine's own floor of %d MB, nothing patched", g_cfg.meshBudgetMb, kMinMeshBudgetMb);
        return;
    }

    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    auto nt = (IMAGE_NT_HEADERS64*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != kPatchedBuildTimeDateStamp || nt->OptionalHeader.SizeOfImage != kPatchedBuildSizeOfImage) {
        Log("[mesh] this is not the game build the mesh budget patch was written for (stamp %08X, image %08X), nothing patched",
            (unsigned)nt->FileHeader.TimeDateStamp, (unsigned)nt->OptionalHeader.SizeOfImage);
        return;
    }

    for (const CodeRegion& region : kRegions) {
        if (Fnv1a64(base + region.rva, region.length) != region.fnv1a64) {
            Log("[mesh] %s at rva 0x%07X is not the code this was written against, nothing patched", region.what, (unsigned)region.rva);
            return;
        }
    }

    const CodeSite& site = kSiteCanonical;
    BYTE* instruction = base + site.rva;
    int32_t rel = 0;
    memcpy(&rel, instruction + site.dispOffset, 4);
    if ((int64_t)site.rva + site.length + rel != (int64_t)site.target) {
        Log("[mesh] %s at rva 0x%07X does not point where it should, nothing patched", site.what, (unsigned)site.rva);
        return;
    }

    const size_t page = 0x1000;
    BYTE* cave = AllocNear(instruction, 2 * page);
    if (!cave) {
        Log("[mesh] no free memory within reach of the exe, nothing patched");
        return;
    }
    BYTE* budgetSlot = cave + page;
    int32_t budgetMb = g_cfg.meshBudgetMb;
    memcpy(budgetSlot, &budgetMb, 4);

    Emitter emit{ cave };
    emit.LoadEax(budgetSlot);
    emit.JumpTo(base + site.target);

    DWORD old = 0;
    if (!VirtualProtect(cave, page, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[mesh] could not make the stub executable, nothing patched");
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), cave, page);

    int64_t stubRel = cave - (instruction + site.length);
    if (stubRel > INT32_MAX || stubRel < INT32_MIN) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[mesh] the stub is out of reach of %s, nothing patched", site.what);
        return;
    }
    if (!VirtualProtect(instruction, site.length, PAGE_EXECUTE_READWRITE, &old)) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[mesh] could not make the exe's code writable, nothing patched");
        return;
    }
    int32_t newRel = (int32_t)stubRel;
    memcpy(instruction + site.dispOffset, &newRel, 4);
    DWORD ignored = 0;
    VirtualProtect(instruction, site.length, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), instruction, site.length);

    Log("[mesh] the mesh streamer budget is %d MB whenever force_canonical_pool_sizes fixes the pool sizes. "
        "The game log's own [Mesh Streamer] line still names the canonical size it replaced.", budgetMb);
}
