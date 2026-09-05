#pragma once
#include "acevo/common.h"

// Times the game's controller polling (XInput, DirectInput) to see whether input
// polling eats frame time after a window or device switch (BUG-013). Counters are
// read and reset by the timeline thread once a second.
extern std::atomic<uint64_t> g_inputCalls;   // polls in the current second
extern std::atomic<uint64_t> g_inputUs;      // time spent inside them
extern std::atomic<uint64_t> g_inputMaxUs;   // slowest single poll

// Hooks the exe's imports. Call once everything is loaded (first DirectStorage use).
void InstallInputProbe();
