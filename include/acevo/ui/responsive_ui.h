#pragma once
#include "acevo/common.h"

// The responsive UI, [engine] responsive_ui, on by default: the fixes that keep the game's menus smooth.
// The stylesheet part is served by the overlay, everything else is installed from here. Call once from
// DllMain before InstallCohtmlHooks, while the process is still single threaded.
void InstallResponsiveUi();

// For the UI probe, the resource work calls the game's frame thread handed to the mod's thread since the
// previous call.
uint32_t ResponsiveUiTakeMovedWork();
