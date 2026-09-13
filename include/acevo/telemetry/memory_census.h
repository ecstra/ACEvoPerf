#pragma once
#include "acevo/common.h"

// Memory census for BUG-016. The game logs only its total committed memory, and that total grows by
// about a gigabyte with the first track and by hundreds of MB with every track after it, with or
// without the mod. This names what the total is made of and which code keeps it.
//
// Diagnostics, off unless [developer] memory_census=1. Called once from DllMain.
void InstallMemoryCensus();

// Once a second from the timeline thread, writes acevo_perf_memory.csv every stats interval.
void MemoryCensusTick();
