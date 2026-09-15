#pragma once
#include "acevo/common.h"

// Stops the UI engine restyling a whole element when one of its children is removed, restyling only the
// children whose rules look at their position, part of the responsive UI. Called once from DllMain while the
// process is still single threaded, and patches nothing unless cohtml.WindowsDesktop.dll is the build it was
// written for. See the source for what goes wrong in the engine and what is patched.
void InstallChildRemovalFix();

struct ChildRemovalCounts {
    uint32_t narrowed;      // removals that marked only the children that look at their position
    uint32_t marked;        // children those removals marked
    uint32_t full;          // removals left to the engine's own invalidation
};

// The counts since the last call, for the UI probe.
ChildRemovalCounts ChildRemovalFixTakeCounts();
