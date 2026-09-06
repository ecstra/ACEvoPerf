#pragma once
#include "acevo/common.h"

// Write the [flags] values from the ini into the engine's gflags storage.
// Called once early (DllMain) and once late (first DirectStorage use).
// Values set to `auto` are skipped there and written by ApplyAutoFlags once
// the card is known (see render/adapter).
void ApplyFlags(const char* phase);
void ApplyAutoFlags(int tilePoolMb);
