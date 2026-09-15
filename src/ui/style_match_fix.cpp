#include "acevo/ui/style_match_fix.h"
#include "acevo/core/code_patch.h"
#include "acevo/core/log.h"

// Read from cohtml.WindowsDesktop.dll 1.61.0.3 and the game's stylesheets of 0.9.1, measured with the UI
// probe on 2026-09-15.
//
// Styling an element matches it against the page's rules. Cohtml files each rule by the first simple
// selector of its rightmost compound. A rule that is only one class, id, tag or * goes into a map the
// element's own names are looked up in, a longer rule that starts with a class into a class map, and
// every other rule into one list (+0x128 in the rule set, filed at 0x3E9C40) that the collector at
// 0x3EEC00 runs the full matcher on for every element. In the game's stylesheets that list holds 2,142
// rules, 1,411 starting with a tag and 673 with an id, so every element runs the matcher 2,142 times to
// find the few rules of its own tag and id. The probe's sampler found that loop on the stack in 40 to 67
// percent of Cohtml's style and layout work, on every page, which is most of what a page open waits for.
//
// A tag compare against a custom element (the game's ks- elements) asks the element for its name, and
// the name getter (0x1AE4C0) copies the name into a new string and upper cases it one call a character
// before the compare (0x3F9D70) runs and the copy is freed. About 700 of the listed rules and 100 of the
// lone tag rules start with a custom tag, so every custom element paid for some 800 copies.
//
// The stubs change what is called, never what is found.
//   The list loop skips a rule whose first simple selector is a tag or an id the element does not have,
//   which is the first check the matcher (0x3ED9D0) makes and fails on, so the skipped calls would have
//   returned no match and changed nothing.
//   The matcher's custom tag compare hands the compare the name where the element keeps it (+0x238, the
//   string the getter copies) instead of the upper cased copy. The compare folds A to Z to lower case on
//   both sides, so the result is the same. An element with another name getter takes Cohtml's own path.
//   The loop over lone tag rules in 0x3EE470 does the same with its inlined compare.
//   The two loops of 0x3EF520 that check the rules holding a state pseudo-class skip a compound whose
//   first simple selector is a tag or an id the element does not have, as the list loop does.

static const DWORD kCohtmlTimeDateStamp = 0x675439C7;
static const DWORD kCohtmlSizeOfImage = 0x0070F000;

struct Region {
    uint32_t rva;
    uint32_t length;
    uint64_t fnv1a64;
    const char* what;
};

static const Region kRegions[] = {
    { 0x3EEC00, 0x26A, 0x59624DD8D5931EA0ull, "the collector of the listed rules" },
    { 0x3EEE70, 0x87, 0x342B1BFB18E9C5C8ull, "the rule matcher" },
    { 0x3ED9D0, 0x129, 0x85B684A55965371Dull, "the simple selector matcher" },
    { 0x3EDC48, 0x20, 0xD197F5B039DF1D46ull, "the simple selector matcher's jump table" },
    { 0x1AE4C0, 0x48, 0xBF112458BCEE183Dull, "the custom element name getter" },
    { 0x3F9D70, 0x9D, 0xFAD9E572BED1A8CCull, "the tag name compare" },
    { 0x3EE470, 0x78A, 0x2CEAE963E649E098ull, "the collector of the lone rules" },
    { 0x3EF6B5, 0x11E, 0x3CA6F2A1A69A127Eull, "the loops over the state rules" },
    { 0x3E9C40, 0x5C, 0x063F58A1D564D686ull, "the rule filing" },
};

static const uint32_t kRvaNameGetter = 0x1AE4C0;

// Replaces the loop at 0x3EEDF2. rbx walks the listed rules to rdi, the element is at [rbp+0x6F] and the
// matcher's context at [rbp-0x29], as 0x3EEC00 lays them out.
static const BYTE kListLoopStub[] = {
    /*00*/ 0x48, 0x8B, 0x13,                               // mov rdx, [rbx]                  the rule
    /*03*/ 0x0F, 0xB6, 0x42, 0x20,                         // movzx eax, byte ptr [rdx+0x20]   its first simple selector's type
    /*07*/ 0x3C, 0x01,                                     // cmp al, 1                        a tag
    /*09*/ 0x75, 0x0E,                                     // jne 0x19
    /*0B*/ 0x48, 0x8B, 0x4D, 0x6F,                         // mov rcx, [rbp+0x6F]              the element
    /*0F*/ 0x8A, 0x42, 0x28,                               // mov al, [rdx+0x28]
    /*12*/ 0x3A, 0x41, 0x28,                               // cmp al, [rcx+0x28]               the element's tag
    /*15*/ 0x75, 0x20,                                     // jne 0x37
    /*17*/ 0xEB, 0x15,                                     // jmp 0x2E
    /*19*/ 0x3C, 0x02,                                     // cmp al, 2                        an id
    /*1B*/ 0x75, 0x11,                                     // jne 0x2E
    /*1D*/ 0x48, 0x8B, 0x4D, 0x6F,                         // mov rcx, [rbp+0x6F]
    /*21*/ 0x48, 0x8B, 0x42, 0x28,                         // mov rax, [rdx+0x28]
    /*25*/ 0x48, 0x3B, 0x81, 0xE8, 0x01, 0x00, 0x00,       // cmp rax, [rcx+0x1E8]             the element's id
    /*2C*/ 0x75, 0x09,                                     // jne 0x37
    /*2E*/ 0x48, 0x8D, 0x4D, 0xD7,                         // lea rcx, [rbp-0x29]
    /*32*/ 0xE8, 0, 0, 0, 0,                               // call the rule matcher
    /*37*/ 0x48, 0x83, 0xC3, 0x08,                         // add rbx, 8
    /*3B*/ 0x48, 0x3B, 0xDF,                               // cmp rbx, rdi
    /*3E*/ 0x75, 0xC0,                                     // jne 0x00
    /*40*/ 0xE9, 0, 0, 0, 0,                               // jmp past the loop
};

// Replaces mov rax, [r11] / lea rdx, [rsp+0x20] at 0x3EDA76, where the matcher asks a custom element for
// its name. rbx is the simple selector, r11 the element.
static const BYTE kTagCompareStub[] = {
    /*00*/ 0x49, 0x8B, 0x03,                               // mov rax, [r11]
    /*03*/ 0x49, 0xBA, 0, 0, 0, 0, 0, 0, 0, 0,             // mov r10, the custom element name getter
    /*0D*/ 0x4C, 0x39, 0x90, 0x30, 0x01, 0x00, 0x00,       // cmp [rax+0x130], r10
    /*14*/ 0x75, 0x29,                                     // jne 0x3F
    /*16*/ 0x41, 0xF6, 0x83, 0x38, 0x02, 0x00, 0x00, 0x01, // test byte ptr [r11+0x238], 1     the name kept in place
    /*1E*/ 0x49, 0x8D, 0x93, 0x39, 0x02, 0x00, 0x00,       // lea rdx, [r11+0x239]
    /*25*/ 0x75, 0x07,                                     // jne 0x2E
    /*27*/ 0x49, 0x8B, 0x93, 0x48, 0x02, 0x00, 0x00,       // mov rdx, [r11+0x248]
    /*2E*/ 0x48, 0x8D, 0x4B, 0x10,                         // lea rcx, [rbx+0x10]
    /*32*/ 0xE8, 0, 0, 0, 0,                               // call the tag name compare
    /*37*/ 0x0F, 0xB6, 0xD8,                               // movzx ebx, al
    /*3A*/ 0xE9, 0, 0, 0, 0,                               // jmp to the matcher's return
    /*3F*/ 0x48, 0x8D, 0x54, 0x24, 0x20,                   // lea rdx, [rsp+0x20]
    /*44*/ 0xE9, 0, 0, 0, 0,                               // jmp back to the name getter call
};

// Replaces mov r8, [rbp+0x38] / lea rdx, [rbp-0x68] at 0x3EE9DF, the lone tag rule loop's name getter
// call. The compare that follows takes the element's name in r10 and leaves the frame's copy flag alone.
static const BYTE kLoneTagStub[] = {
    /*00*/ 0x4C, 0x8B, 0x45, 0x38,                         // mov r8, [rbp+0x38]               the element
    /*04*/ 0x49, 0x8B, 0x00,                               // mov rax, [r8]
    /*07*/ 0x49, 0xBA, 0, 0, 0, 0, 0, 0, 0, 0,             // mov r10, the custom element name getter
    /*11*/ 0x4C, 0x39, 0x90, 0x30, 0x01, 0x00, 0x00,       // cmp [rax+0x130], r10
    /*18*/ 0x75, 0x1D,                                     // jne 0x37
    /*1A*/ 0x41, 0xF6, 0x80, 0x38, 0x02, 0x00, 0x00, 0x01, // test byte ptr [r8+0x238], 1
    /*22*/ 0x4D, 0x8D, 0x90, 0x39, 0x02, 0x00, 0x00,       // lea r10, [r8+0x239]
    /*29*/ 0x75, 0x07,                                     // jne 0x32
    /*2B*/ 0x4D, 0x8B, 0x90, 0x48, 0x02, 0x00, 0x00,       // mov r10, [r8+0x248]
    /*32*/ 0xE9, 0, 0, 0, 0,                               // jmp to the compare
    /*37*/ 0x48, 0x8D, 0x55, 0x98,                         // lea rdx, [rbp-0x68]
    /*3B*/ 0xE9, 0, 0, 0, 0,                               // jmp back to the name getter call
};

// Replaces mov rbx, rsi / mov dword ptr [rbp+0xB70], 1 at the head of each state rule loop. rdi is the
// compound, 72 bytes with its first simple selector in place, r14 the element.
static const BYTE kStateLoopStub[] = {
    /*00*/ 0x0F, 0xB6, 0x07,                               // movzx eax, byte ptr [rdi]
    /*03*/ 0x3C, 0x01,                                     // cmp al, 1
    /*05*/ 0x75, 0x0B,                                     // jne 0x12
    /*07*/ 0x8A, 0x47, 0x08,                               // mov al, [rdi+8]
    /*0A*/ 0x41, 0x3A, 0x46, 0x28,                         // cmp al, [r14+0x28]
    /*0E*/ 0x75, 0x25,                                     // jne 0x35
    /*10*/ 0xEB, 0x11,                                     // jmp 0x23
    /*12*/ 0x3C, 0x02,                                     // cmp al, 2
    /*14*/ 0x75, 0x0D,                                     // jne 0x23
    /*16*/ 0x48, 0x8B, 0x47, 0x08,                         // mov rax, [rdi+8]
    /*1A*/ 0x49, 0x3B, 0x86, 0xE8, 0x01, 0x00, 0x00,       // cmp rax, [r14+0x1E8]
    /*21*/ 0x75, 0x12,                                     // jne 0x35
    /*23*/ 0x48, 0x8B, 0xDE,                               // mov rbx, rsi
    /*26*/ 0xC7, 0x85, 0x70, 0x0B, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // mov dword ptr [rbp+0xB70], 1
    /*30*/ 0xE9, 0, 0, 0, 0,                               // jmp back into the loop
    /*35*/ 0xE9, 0, 0, 0, 0,                               // jmp to the next compound
};

// UNWIND_INFO for each stub, the frame of the function it stands in, so a stack walk through a stub,
// from a callee or from a thread suspended inside it, finds its way out. The ops are Cohtml's own for the
// place the stub replaces, with a chained chunk's ops and its parent's in one list.
// 0x3EEC00 at its rule loop, push rbp, push rbx, push rdi, sub rsp 0xA0.
static const BYTE kListLoopUnwind[] = {
    0x01, 0x00, 0x05, 0x00,
    0x00, 0x01, 0x14, 0x00,     // alloc 0xA0
    0x00, 0x70,                 // push rdi
    0x00, 0x30,                 // push rbx
    0x00, 0x50,                 // push rbp
    0x00, 0x00,
};
// 0x3ED9D0, push rbx, sub rsp 0x40.
static const BYTE kTagCompareUnwind[] = {
    0x01, 0x00, 0x02, 0x00,
    0x00, 0x72,                 // alloc 0x40
    0x00, 0x30,                 // push rbx
};
// 0x3EE470 in the chunk that keeps r15, r13, r12 and rsi in the frame, push rbp, push rbx, push rdi,
// push r14, sub rsp 0x108.
static const BYTE kLoneTagUnwind[] = {
    0x01, 0x00, 0x0E, 0x00,
    0x00, 0xF4, 0x1E, 0x00,     // r15 at 0xF0
    0x00, 0xD4, 0x1F, 0x00,     // r13 at 0xF8
    0x00, 0xC4, 0x20, 0x00,     // r12 at 0x100
    0x00, 0x64, 0x28, 0x00,     // rsi at 0x140
    0x00, 0x01, 0x21, 0x00,     // alloc 0x108
    0x00, 0xE0,                 // push r14
    0x00, 0x70,                 // push rdi
    0x00, 0x30,                 // push rbx
    0x00, 0x50,                 // push rbp
};
// 0x3EF520, rbx kept in the frame, push rbp, rsi, rdi, r12 to r15, sub rsp 0xC30.
static const BYTE kStateLoopUnwind[] = {
    0x01, 0x00, 0x0B, 0x00,
    0x00, 0x34, 0x90, 0x01,     // rbx at 0xC80
    0x00, 0x01, 0x86, 0x01,     // alloc 0xC30
    0x00, 0xF0,                 // push r15
    0x00, 0xE0,                 // push r14
    0x00, 0xD0,                 // push r13
    0x00, 0xC0,                 // push r12
    0x00, 0x70,                 // push rdi
    0x00, 0x60,                 // push rsi
    0x00, 0x50,                 // push rbp
    0x00, 0x00,
};

struct Rel32 {
    size_t at;          // the E8 or E9 in the stub
    uint32_t rva;
};

struct Stub {
    const char* what;
    uint32_t rva;       // where the jump to the stub goes
    uint32_t length;    // the whole instructions it replaces
    const BYTE* code;
    size_t size;
    Rel32 rel32[3];
    size_t getterAt;    // the name getter's address in the stub, 0 for none
    const BYTE* unwind;
    size_t unwindSize;
};

static const Stub kStubs[] = {
    { "the listed rule loop", 0x3EEDF2, 21, kListLoopStub, sizeof kListLoopStub,
      { { 0x32, 0x3EEE70 }, { 0x40, 0x3EEE07 } }, 0, kListLoopUnwind, sizeof kListLoopUnwind },
    { "the matcher's custom tag compare", 0x3EDA76, 8, kTagCompareStub, sizeof kTagCompareStub,
      { { 0x32, 0x3F9D70 }, { 0x3A, 0x3EDAF0 }, { 0x44, 0x3EDA7E } }, 0x05, kTagCompareUnwind, sizeof kTagCompareUnwind },
    { "the lone tag rule compare", 0x3EE9DF, 8, kLoneTagStub, sizeof kLoneTagStub,
      { { 0x32, 0x3EEA06 }, { 0x3B, 0x3EE9E7 } }, 0x09, kLoneTagUnwind, sizeof kLoneTagUnwind },
    { "the hover rule loop", 0x3EF6D1, 13, kStateLoopStub, sizeof kStateLoopStub,
      { { 0x30, 0x3EF6DE }, { 0x35, 0x3EF72F } }, 0, kStateLoopUnwind, sizeof kStateLoopUnwind },
    { "the second state rule loop", 0x3EF760, 13, kStateLoopStub, sizeof kStateLoopStub,
      { { 0x30, 0x3EF76D }, { 0x35, 0x3EF7BE } }, 0, kStateLoopUnwind, sizeof kStateLoopUnwind },
};

static const BYTE* g_cave = nullptr;
static const RUNTIME_FUNCTION* g_functions = nullptr;
static DWORD g_functionCount = 0;
static const int kStubCount = (int)(sizeof kStubs / sizeof kStubs[0]);

void InstallStyleMatchFix()
{
    BYTE* cohtml = (BYTE*)GetModuleHandleW(L"cohtml.WindowsDesktop.dll");
    if (!cohtml) {
        Log("[styles] cohtml.WindowsDesktop.dll is not loaded, nothing patched");
        return;
    }
    auto nt = (IMAGE_NT_HEADERS64*)(cohtml + ((IMAGE_DOS_HEADER*)cohtml)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != kCohtmlTimeDateStamp || nt->OptionalHeader.SizeOfImage != kCohtmlSizeOfImage) {
        Log("[styles] this is not the Cohtml build the style matching fix was written for (stamp %08X, image %08X), nothing patched",
            (unsigned)nt->FileHeader.TimeDateStamp, (unsigned)nt->OptionalHeader.SizeOfImage);
        return;
    }
    for (const Region& region : kRegions) {
        if (Fnv1a64(cohtml + region.rva, region.length) != region.fnv1a64) {
            Log("[styles] %s at rva 0x%06X is not the code this was written against, nothing patched", region.what, (unsigned)region.rva);
            return;
        }
    }

    const size_t page = 0x1000;
    BYTE* cave = AllocNear(cohtml + kStubs[0].rva, page);
    if (!cave) {
        Log("[styles] no free memory within reach of Cohtml, nothing patched");
        return;
    }

    // The stubs, then the unwind data and the function table that points at it, all in the one page.
    BYTE* at = cave;
    BYTE* stubAt[kStubCount] = {};
    BYTE jumps[kStubCount][32] = {};
    const BYTE* getter = cohtml + kRvaNameGetter;
    bool reachable = true;
    for (int i = 0; i < kStubCount; ++i) {
        const Stub& stub = kStubs[i];
        stubAt[i] = at;
        memcpy(at, stub.code, stub.size);
        for (const Rel32& fixup : stub.rel32) {
            if (!fixup.rva) continue;
            reachable &= EncodeRel32(at[fixup.at], at + fixup.at, cohtml + fixup.rva, at + fixup.at);
        }
        if (stub.getterAt) memcpy(at + stub.getterAt, &getter, 8);
        memset(jumps[i], 0xCC, stub.length);
        reachable &= EncodeRel32(0xE9, cohtml + stub.rva, at, jumps[i]);
        at += (stub.size + 15) & ~(size_t)15;
    }

    RUNTIME_FUNCTION functions[kStubCount] = {};
    int functionCount = 0;
    for (int i = 0; i < kStubCount; ++i) {
        const Stub& stub = kStubs[i];
        if (!stub.unwind) continue;
        memcpy(at, stub.unwind, stub.unwindSize);
        functions[functionCount].BeginAddress = (DWORD)(stubAt[i] - cave);
        functions[functionCount].EndAddress = (DWORD)(stubAt[i] - cave + stub.size);
        functions[functionCount].UnwindData = (DWORD)(at - cave);
        functionCount++;
        at += (stub.unwindSize + 3) & ~(size_t)3;
    }
    RUNTIME_FUNCTION* table = (RUNTIME_FUNCTION*)at;
    memcpy(table, functions, functionCount * sizeof(RUNTIME_FUNCTION));

    DWORD old = 0;
    if (!reachable || !VirtualProtect(cave, page, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[styles] the stubs could not be placed within reach of Cohtml, nothing patched");
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), cave, page);
    if (!RtlAddFunctionTable(table, functionCount, (DWORD64)cave)) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[styles] could not register the stubs' unwind data, nothing patched");
        return;
    }
    g_cave = cave;
    g_functions = table;
    g_functionCount = (DWORD)functionCount;

    int written = 0;
    for (int i = 0; i < kStubCount; ++i) {
        if (WriteCode(cohtml + kStubs[i].rva, jumps[i], kStubs[i].length)) {
            written++;
            continue;
        }
        Log("[styles] could not patch %s at rva 0x%06X, it matches as the game does", kStubs[i].what, (unsigned)kStubs[i].rva);
    }
    Log("[styles] style matching fix on at %d of %d places, elements skip the rules they cannot match and custom element names are compared in place",
        written, kStubCount);
}

bool StyleMatchFixUnwind(const BYTE** base, size_t* size, const RUNTIME_FUNCTION** functions, DWORD* count)
{
    if (!g_cave) return false;
    *base = g_cave;
    *size = 0x1000;
    *functions = g_functions;
    *count = g_functionCount;
    return true;
}
