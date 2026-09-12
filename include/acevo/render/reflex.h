#pragma once
#include "acevo/common.h"

// NVIDIA Reflex, added to a game that has none.
//
// Reflex does not cap the frame rate. It stops the CPU running further ahead of the
// GPU than it usefully can, by sleeping at the start of a frame for however long the
// driver knows the frame will spend queued anyway. Fewer queued frames means the input
// that produced a frame is newer, and one late frame is less able to drag the next one
// with it. The frame rate stays unlocked, which is the point: `minimumIntervalUs` is
// left at zero so nothing here is a limiter.
//
// NVIDIA only, and silently idle on anything else: without nvapi64.dll, without an
// NVIDIA adapter, or on a driver too old for the entry points, none of this runs.
namespace reflex {

// Called when the swap chain is hooked. Finds nvapi, resolves the entry points and
// tells the driver what mode to run in. Safe to call more than once.
void OnSwapChain(IUnknown* swapChain);

// Called from the present hook once the real Present has returned, which is the
// boundary between one frame and the next and the only frame start a proxy can see.
void OnFrameBegin();

} // namespace reflex
