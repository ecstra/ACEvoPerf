#pragma once
#include "acevo/common.h"

// The game makes one Cohtml library, one system and the views through vtables, reached from the one
// export the exe imports, Library::Initialize, and drives the UI once a frame through its own UI object.
// The responsive UI and the developer UI probe both need those, so the hooks live here once and call
// whoever registered, in the order they registered. Register from DllMain, before the game starts its UI.
typedef void (*CohtmlLibraryListener)(void* library);
// `mainView` marks the menu and HUD view. It is told by size rather than by being first, because the game
// makes the car displays again at every session load and can tear the menu view down and make it again,
// after which an ordinal would never point at it a second time.
typedef void (*CohtmlViewListener)(void* view, int number, unsigned width, unsigned height, bool mainView);
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

// True once the game UI's frame end slot is actually wrapped, so the frame end listeners run. The two
// checks are independent, the frame slots are the exe's and the rest is the UI engine's, so a build that
// moves one and not the other leaves this false while the Cohtml hooks install. Anything that needs what
// a frame end listener learns has to ask before it starts. Call it from a listener, not from DllMain,
// because InstallCohtmlHooks runs after the listeners register.
bool UiFrameEndHooked();

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
