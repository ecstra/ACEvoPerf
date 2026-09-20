#pragma once
#include "acevo/common.h"

// The card the game renders on, read from the DXGI factory the game creates (before the
// renderer sizes its pools and before DirectStorage starts), and what follows from it.
int AutoTilePoolMb(uint64_t vramMb);      // tile pool for a card with this much dedicated memory
int AutoStagingMb(uint64_t vramMb);       // DirectStorage staging buffer for it
void ResolveAutoSizes(IDXGIFactory1* factory);   // fills in every ini value set to auto, once

// Whether the adapter the sizes were picked from is the one the game ended up rendering on.
// Only knowable once the game's device exists, which is after the pools are made, so a
// disagreement is logged with what to set by hand rather than corrected.
void CheckAutoSizeAdapter(IDXGIFactory1* factory, IUnknown* device);

// Whether the window's monitor belongs to the render adapter. When it does not, every frame
// is copied to the other adapter before it is shown, which the log then says.
void LogDisplayOwner(IDXGIFactory1* factory, IUnknown* device, HWND hwnd);
