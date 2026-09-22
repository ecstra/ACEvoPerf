#pragma once
#include "acevo/common.h"

// FNV-1a over a range of loaded code, to check the bytes a patch depends on before touching them.
uint64_t Fnv1a64(const BYTE* p, size_t n);

// Read and write memory within reach of a 32 bit displacement from `anchor`, for stubs a patched
// jump or call can reach. Null when no free range is near enough.
BYTE* AllocNear(BYTE* anchor, size_t size);

// Writes a five byte jmp (0xE9) or call (0xE8) into `out`, for code that will run at `from`. False when
// `destination` is out of reach.
bool EncodeRel32(BYTE opcode, const BYTE* from, const BYTE* destination, BYTE* out);

// Copies `code` over loaded code, restoring the page protection.
//
// Only safe while the game is still one thread, which it is for the whole of DLL_PROCESS_ATTACH
// and never again. There is no thread suspension here and a multi byte patch is not atomic, so a
// thread executing those bytes mid write runs half of the old instruction and half of the new one.
// Every caller today is on the attach path. One that is not gets a warning in the log, see the
// source for what it would take to make it genuinely safe.
bool WriteCode(BYTE* at, const BYTE* code, size_t length);

// Called as the last thing attach does, from which point the game has threads of its own.
void CodePatchingIsNowUnsafe();

// Whether that point has passed. `WriteCode` asks for itself. The three places that change page
// protection and copy over code without going through it, in `engine/streamer`, `ui/restyle_fix`
// and `ui/ui_probe`, ask on their own behalf, since the hazard is theirs too and nothing else
// would catch them.
bool CodePatchingIsLate();
