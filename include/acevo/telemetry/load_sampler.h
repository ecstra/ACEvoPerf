#pragma once
#include "acevo/common.h"

// Sampling profiler for a session load.
//
// Three quarters of a session load is the engine's `Track resources streaming`
// phase, and the drive is not what it waits on: the whole load reads 3.3 GB,
// which the drive delivers in about a second out of the thirteen the phase takes
// (TODO-013). So the time goes somewhere inside the resource workers, and this
// names it: a helper thread finds the threads actually burning CPU, reads their
// instruction pointer a few thousand times a second, and tallies what they were
// doing by module and by function.
//
// Diagnostics, off unless [profile] load_sampler=1, and it suspends game threads
// to read them, so it never ships enabled.
void StartLoadSampler();
void StopLoadSampler();
