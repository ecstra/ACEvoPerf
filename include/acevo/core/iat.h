#pragma once
#include "acevo/common.h"

// Replace every import slot of `mod` that currently holds `target` with `replacement`.
int PatchIatByAddress(HMODULE mod, void* target, void* replacement);

// Patch the import slots of every loaded module for a kernel32 or kernelbase export.
// Pass hook = nullptr to only resolve the original address into *orig.
int PatchEverywhere(const char* fn, void* hook, void** orig);

// Swap one COM vtable entry, remembering the original once.
void HookVtableSlot(void** vt, int idx, void* hook, void** orig, const char* what);
