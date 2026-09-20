#pragma once
#include "acevo/common.h"

// The game makes one Cohtml library, one system and the views through vtables, reached from the one
// export the exe imports, Library::Initialize, and drives the UI once a frame through its own UI object.
// The responsive UI and the developer UI probe both need those, so the hooks live here once and call
// whoever registered, in the order they registered. Register from DllMain, before the game starts its UI.
typedef void (*CohtmlLibraryListener)(void* library);
typedef void (*CohtmlViewListener)(void* view, int number, unsigned width, unsigned height);
// The game's UI frame post, with the UI clock it hands Cohtml, on the thread that posts it.
typedef void (*UiFramePostListener)(float uiClock);
// The game's UI frame end, which waits for the frame's UI job, called just before and just after it on
// the thread that waits.
typedef void (*UiFrameEndListener)(bool after);

void AddCohtmlLibraryListener(CohtmlLibraryListener listener);
void AddCohtmlViewListener(CohtmlViewListener listener);
void AddUiFramePostListener(UiFramePostListener listener);
void AddUiFrameEndListener(UiFrameEndListener listener);

// Patches the exe's import of Library::Initialize and the game UI's frame slots the first time it is
// called, later calls do nothing. The frame slots are left alone on a game build they were not read from,
// and nothing that reaches a Cohtml vtable is hooked at all unless the loaded UI engine is the version the
// slots below were read from, so a listener only ever runs on the build its slot numbers belong to.
void InstallCohtmlHooks();

// Cohtml vtable slots read from cohtml.WindowsDesktop.dll 1.61.0.3. InstallCohtmlHooks checks the loaded
// engine is that version before any of these is used, because a slot that moved would be an indirect call
// through the wrong method with the wrong signature.
namespace cohtml_slot {
    static const int kLibraryCreateSystem = 1;
    static const int kLibraryStopWorkers = 2;
    static const int kLibraryUninitialize = 3;
    static const int kLibraryExecuteWork = 5;
    static const int kSystemCreateView = 3;
    static const int kViewAdvance = 6;
    static const int kViewAddInitialScript = 61;
}
