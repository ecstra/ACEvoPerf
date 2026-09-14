#pragma once
#include "acevo/common.h"

// The developer UI probe, [developer] ui_probe. It times the game's Cohtml views and UI work, follows
// the UI clock the game hands Cohtml, samples where Cohtml's layout work spends its time, and adds a
// page script to the menu and HUD view that counts what the pages change and applies the page fixes.
void InstallUiProbe();

// Once a second from the timeline thread, logs the [ui] line.
void UiProbeTick();

// For the frames CSV, each returns what accumulated since its previous call.
uint32_t UiProbeTakeEndFrameUs();
uint32_t UiProbeTakeAdvanceUs();
