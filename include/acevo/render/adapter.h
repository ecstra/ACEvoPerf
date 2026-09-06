#pragma once
#include "acevo/common.h"

// The card the game renders on, read from the DXGI factory the game creates (before the
// renderer sizes its pools and before DirectStorage starts), and what follows from it.
int AutoTilePoolMb(uint64_t vramMb);      // tile pool for a card with this much dedicated memory
int AutoStagingMb(uint64_t vramMb);       // DirectStorage staging buffer for it
void ResolveAutoSizes(IDXGIFactory1* factory);   // fills in every ini value set to auto, once

// Whether the window's monitor belongs to the render adapter. When it does not, every frame
// is copied to the other adapter before it is shown, which the log then says.
void LogDisplayOwner(IDXGIFactory1* factory, IUnknown* device, HWND hwnd);
