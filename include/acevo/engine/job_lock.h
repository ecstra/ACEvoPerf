#pragma once
#include "acevo/common.h"

// Replace the engine's spin lock loop with one that reads before it writes. Called once from
// DllMain, while the process is still single threaded. Does nothing unless [engine]
// job_lock_fix=1. See the source for what is patched and why it is safe.
void PatchJobQueueSpinLock();

// The range holding the replacement loops, empty when nothing was patched. The load sampler
// needs it: once a site is patched the hot instruction pointer lives here instead of inside the
// exe, and a spin that stopped being counted would look like a spin that stopped happening.
void JobLockCave(const BYTE** lo, const BYTE** hi);
