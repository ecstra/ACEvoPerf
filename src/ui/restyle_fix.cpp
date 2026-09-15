#include "acevo/ui/restyle_fix.h"
#include "acevo/core/code_patch.h"
#include "acevo/core/log.h"

// Read from cohtml.WindowsDesktop.dll 1.61.0.3 and the game's stylesheets of 0.9.1, measured with the
// UI probe on 2026-09-14.
//
// When the element under the mouse changes, Cohtml walks the element and every ancestor and
// invalidates each one whose style can depend on its state (0x37C7A0, which passes kind 5). The
// invalidation (0x37B690) marks the nodes under the element that the state's invalidation set
// matches. When the page's stylesheets hold any rule with a sibling combinator it also walks every
// sibling that follows the element and marks the nodes under those, whatever feature changed, because
// the only thing it checks is the count of such rules (+0x70 in the rule feature set, gate at
// 0x37B92B). The state's invalidation set covers whole subtrees, so an invalidated row marks every
// row below it.
//
// This is the second half of the UI restyle fix. The first, the stylesheet the overlay serves, stops
// the page containers counting as state dependent. With only this half in, measured, every hover still
// restyled the whole page through those containers. Once only the hovered row counts, this half keeps
// that row from taking the rows below it along.
//
// The game's stylesheets hold one rule with a sibling combinator, ks-leaderboard .realtime-panel
// .focused ~ .leaderboardline .driver-time, and no rule with a pseudo-class to the left of + or ~. A
// state change can only reach a following sibling through such a rule, so the stub below skips the
// sibling walk for state changes and keeps it for classes, attributes and ids, which the leaderboard
// rule needs. The kind is the saved rbp of the only caller (0x37BC60 keeps it in ebp), at [rbp+0x30]
// in 0x37B690's frame.

static const DWORD kGameTimeDateStamp = 0x6A9EC72A;
static const DWORD kGameSizeOfImage = 0x06CDD000;
static const DWORD kCohtmlTimeDateStamp = 0x675439C7;
static const DWORD kCohtmlSizeOfImage = 0x0070F000;

static const uint8_t kStateKind = 5;

struct Region {
    uint32_t rva;
    uint32_t length;
    uint64_t fnv1a64;
    const char* what;
};

static const Region kSiblingGate = { 0x37B92B, 10, 0xE07CB813B3F5ED81ull, "the sibling walk gate" };
static const Region kInvalidatePrologue = { 0x37B690, 0x20, 0x5251724A88F60112ull, "the invalidation's prologue" };
// Up to its call of 0x37B690, which the UI probe may redirect.
static const Region kInvalidateCaller = { 0x37BC60, 0xB1, 0x666FF21FA9FFC325ull, "the invalidation's only caller" };

static const uint32_t kRvaSiblingWalk = 0x37B935;
static const uint32_t kRvaAfterSiblingWalk = 0x37BB21;

// cmp byte ptr [rbp+0x30], 5 / je skip / cmp dword ptr [rcx+0x70], r12d / jbe skip /
// jmp sibling walk / skip: jmp past it
static const BYTE kGateStub[] = {
    0x80, 0x7D, 0x30, kStateKind,
    0x74, 0x0B,
    0x44, 0x39, 0x61, 0x70,
    0x76, 0x05,
    0xE9, 0, 0, 0, 0,
    0xE9, 0, 0, 0, 0,
};
static const size_t kGateStubWalkAt = 12;
static const size_t kGateStubSkipAt = 17;

// jmp rel32 for code that will run at `from`, written into `out`.
static bool EncodeJump(const BYTE* from, const BYTE* destination, BYTE* out)
{
    int64_t rel = destination - (from + 5);
    if (rel > INT32_MAX || rel < INT32_MIN) return false;
    int32_t value = (int32_t)rel;
    out[0] = 0xE9;
    memcpy(out + 1, &value, 4);
    return true;
}

void InstallRestyleFix()
{
    BYTE* game = (BYTE*)GetModuleHandleW(nullptr);
    auto gameNt = (IMAGE_NT_HEADERS64*)(game + ((IMAGE_DOS_HEADER*)game)->e_lfanew);
    if (gameNt->FileHeader.TimeDateStamp != kGameTimeDateStamp || gameNt->OptionalHeader.SizeOfImage != kGameSizeOfImage) {
        Log("[restyle] this is not the game build whose stylesheets the UI restyle fix was checked against, nothing patched");
        return;
    }
    BYTE* cohtml = (BYTE*)GetModuleHandleW(L"cohtml.WindowsDesktop.dll");
    if (!cohtml) {
        Log("[restyle] cohtml.WindowsDesktop.dll is not loaded, nothing patched");
        return;
    }
    auto nt = (IMAGE_NT_HEADERS64*)(cohtml + ((IMAGE_DOS_HEADER*)cohtml)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != kCohtmlTimeDateStamp || nt->OptionalHeader.SizeOfImage != kCohtmlSizeOfImage) {
        Log("[restyle] this is not the Cohtml build the UI restyle fix was written for (stamp %08X, image %08X), nothing patched",
            (unsigned)nt->FileHeader.TimeDateStamp, (unsigned)nt->OptionalHeader.SizeOfImage);
        return;
    }
    for (const Region* region : { &kSiblingGate, &kInvalidatePrologue, &kInvalidateCaller }) {
        if (Fnv1a64(cohtml + region->rva, region->length) != region->fnv1a64) {
            Log("[restyle] %s at rva 0x%06X is not the code this was written against, nothing patched", region->what, (unsigned)region->rva);
            return;
        }
    }

    const size_t page = 0x1000;
    BYTE* stub = AllocNear(cohtml + kSiblingGate.rva, page);
    if (!stub) {
        Log("[restyle] no free memory within reach of Cohtml, nothing patched");
        return;
    }
    memcpy(stub, kGateStub, sizeof kGateStub);
    BYTE* gate = cohtml + kSiblingGate.rva;
    // The five bytes after the jump are the rest of the original jbe, never reached again.
    BYTE patched[10];
    memset(patched, 0xCC, sizeof patched);
    if (!EncodeJump(stub + kGateStubWalkAt, cohtml + kRvaSiblingWalk, stub + kGateStubWalkAt) ||
        !EncodeJump(stub + kGateStubSkipAt, cohtml + kRvaAfterSiblingWalk, stub + kGateStubSkipAt) ||
        !EncodeJump(gate, stub, patched)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        Log("[restyle] the stub is out of reach of Cohtml, nothing patched");
        return;
    }

    DWORD old = 0;
    if (!VirtualProtect(stub, page, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        Log("[restyle] could not make the stub executable, nothing patched");
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), stub, page);

    if (!VirtualProtect(gate, kSiblingGate.length, PAGE_EXECUTE_READWRITE, &old)) {
        Log("[restyle] could not make Cohtml's code writable, nothing patched");
        return;
    }
    memcpy(gate, patched, sizeof patched);
    DWORD ignored = 0;
    VirtualProtect(gate, kSiblingGate.length, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), gate, kSiblingGate.length);

    Log("[restyle] UI restyle fix on, a hover or other state change no longer restyles every element after the one it happened on");
}
