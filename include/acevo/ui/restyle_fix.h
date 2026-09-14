#pragma once
#include "acevo/common.h"

// Stops the UI engine restyling every element that follows a hovered one. Called once from DllMain
// while the process is still single threaded. Does nothing unless [engine] ui_restyle_fix=1, and
// patches nothing unless the game and cohtml.WindowsDesktop.dll are the builds it was written for.
// See the source for what goes wrong in the engine and what is patched.
void InstallRestyleFix();
