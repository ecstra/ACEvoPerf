#pragma once
#include "acevo/common.h"

typedef HRESULT (WINAPI *PFN_CreateDXGIFactory1)(REFIID, void**);
typedef HRESULT (WINAPI *PFN_CreateDXGIFactory2)(UINT, REFIID, void**);

// Hook the game's DXGI factory creation so swap chains get the frame timing hook.
void InstallDxgiHooks();
