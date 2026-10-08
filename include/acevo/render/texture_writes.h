#pragma once
#include "acevo/common.h"

// Runtime writes into streamed textures, a diagnostic for [engine] streamer_reload_fix, active
// only with [developer] streaming_trace=1. See the source for what counts as a write and why that is
// complete.

// Called for every texture tile request, records the resource and its creation flags once.
void NoteStreamedResource(ID3D12Resource* resource);

// Called once the game's swap chain exists, hooks the command list copy calls. It also resolves
// the DirectStorage cores' module ranges, which is why it has to run before the first hooked copy
// and not merely before the first report: the copy hook reads those ranges and must never look
// them up itself, since that takes the loader lock on the render thread.
void TextureWritesOnSwapChain(IUnknown* deviceOrQueue);

// Once a second from the timeline thread, the [writes] line every stats interval, and a retry of
// the core ranges while either is still missing.
void TextureWritesTick();
