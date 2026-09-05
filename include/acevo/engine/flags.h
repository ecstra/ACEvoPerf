#pragma once
#include "acevo/common.h"

// Write the [flags] values from the ini into the engine's gflags storage.
// Called once early (DllMain) and once late (first DirectStorage use).
void ApplyFlags(const char* phase);
