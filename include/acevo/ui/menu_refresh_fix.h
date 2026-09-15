#pragma once
#include "acevo/common.h"

// Makes the game update its menu every frame during a session, where it updated the menu one frame in
// three, part of the responsive UI. Called once from DllMain while the process is still single threaded,
// and patches nothing unless the game and cohtml.WindowsDesktop.dll are the builds it was written for.
// See the source for what the game does and what is patched.
void InstallMenuRefreshFix();

// Once a second from the timeline thread. With `[developer] hud_schedule_test` it moves the HUD to its
// next update schedule every 10 seconds, otherwise it does nothing.
void MenuRefreshTick();
