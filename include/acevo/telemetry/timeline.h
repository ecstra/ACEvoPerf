#pragma once
#include "acevo/common.h"

// One CSV line per second (fps, hitches, streaming, VRAM, CPU) plus the per frame CSV.
// Starts a thread once, no effect when both outputs are off in the ini.
void StartTimeline();
