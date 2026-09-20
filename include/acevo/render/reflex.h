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

// Called when a swap chain is hooked, which is every swap chain the game's own exe makes
// after the factory hooks went in, on the runs where they went in at all. A swap chain from
// another module's own factory presents through the same patched vtable but never reaches
// here, because only the exe's import table is patched. Finds nvapi, resolves the entry
// points and tells the driver what mode to run in. The D3D12 device behind the swap chain
// decides what happens: no device at all means ignore that swap chain, a device already
// bound means pace whichever of its swap chains is newest, a different device means the
// game was reset or rebuilt and the layer binds to that one instead.
void OnSwapChain(IUnknown* swapChain);

// Called from the present hook once the real Present has returned, which is the
// boundary between one frame and the next and the only frame start a proxy can see.
// The swap chain that presented is passed because the hooks sit in the vtable inside
// dxgi.dll, which every swap chain in the process shares, and pacing on someone else's
// present would sleep twice for one game frame.
void OnFrameBegin(IUnknown* swapChain);

// Whether the layer took, which is only known after OnSwapChain has run the vendor
// check. The present hooks ask so they are not installed for a layer that is idle.
bool Active();

} // namespace reflex
