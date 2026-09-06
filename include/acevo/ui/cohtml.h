#pragma once
#include "acevo/common.h"

// The game's menus and HUD run on Coherent Gameface (Cohtml). The exe reaches the engine through
// one import, Library::Initialize, and from there through C++ interfaces. This hook follows that
// chain (library, system, views), logs what the game asks for and, with `[ui] inspector_port`
// set, enables the engine's Chrome DevTools inspector on that port.
void InstallCohtmlHooks();
void CohtmlWorkTick();      // called once a second by the timeline thread, logs the work split by thread
