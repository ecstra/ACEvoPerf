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
bool WriteCode(BYTE* at, const BYTE* code, size_t length);
