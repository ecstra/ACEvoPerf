#pragma once
#include "acevo/common.h"

// Diagnostics: count the C++ exceptions the game throws, by throw site and type, and log
// the busiest sites every ten seconds. `[log] throw_log=1`.
void InstallThrowLog();
void ThrowLogTick();     // called once a second by the timeline thread
