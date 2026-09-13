#pragma once
#include "acevo/common.h"

// The engine's texture streamer, hooked in memory. Called once from DllMain while the process is
// still single threaded. Does nothing unless [log] streaming_trace=1 or [engine]
// streamer_reload_fix=1, and patches nothing unless every byte it depends on matches the build it
// was written against. See the source for what goes wrong in the engine and what is patched.
void InstallStreamerHooks();

// Once a second from the timeline thread: writes the trace rows, and the [streamer] summary line
// every stats interval.
void StreamerTick();

// The summary line one last time and the remaining trace rows, at process exit.
void StreamerDetach();
