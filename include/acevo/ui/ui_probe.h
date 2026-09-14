#pragma once
#include "acevo/common.h"

// The developer UI probe, [developer] ui_probe. It times the game's Cohtml views and UI work,
// adds a page script to the menu and HUD view that switches the page fixes on every other visit
// of a page, and makes every UI view update every frame in alternating 20 second blocks.
void InstallUiProbe();

// Once a second from the timeline thread, logs the [ui] line and flips the every view block.
void UiProbeTick();

// For the frames CSV, each returns what accumulated since its previous call.
uint32_t UiProbeTakeEndFrameUs();
uint32_t UiProbeTakeAdvanceUs();
bool UiProbeEveryView();
