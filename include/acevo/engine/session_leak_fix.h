#pragma once
#include "acevo/common.h"

// Frees the finished sessions the game keeps in memory for the rest of the process, each held by a reference
// cycle between its local server connection and its game mode (BUG-016). Called once from DllMain while the
// process is still single threaded, and patches nothing unless every byte it depends on matches the build it
// was written for. See the source for the cycle and what is patched.
void InstallSessionLeakFix();
