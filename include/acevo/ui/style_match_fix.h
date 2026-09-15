#pragma once
#include "acevo/common.h"

// Makes the UI engine's style matching skip the rules an element cannot match and compare custom element
// names without copying them, part of the responsive UI. Called once from DllMain while the process is
// still single threaded, and patches nothing unless cohtml.WindowsDesktop.dll is the build it was written
// for. See the source for what the engine does and what is patched.
void InstallStyleMatchFix();

// The stubs' page and its function table, for the UI probe's sampler to walk stacks through them. False
// when the fix is not installed.
bool StyleMatchFixUnwind(const BYTE** base, size_t* size, const RUNTIME_FUNCTION** functions, DWORD* count);
