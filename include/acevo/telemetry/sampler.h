#pragma once
#include "acevo/common.h"

// Sampling profiler for the render thread: a helper thread reads the instruction pointer of
// the thread that presents, several thousand times a second, and counts which module it was
// in. The counts are cut per frame and written to acevo_perf_samples.csv, so the module mix
// of the slow frames can be compared with the rest. Diagnostics, off unless [profile] sampler=1.
void StartSampler();                   // once everything is loaded
void SamplerOnPresent(double frameMs); // from the present hook, on the render thread
