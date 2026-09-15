#include "acevo/ui/responsive_ui.h"
#include "acevo/core/config.h"
#include "acevo/core/iat.h"
#include "acevo/core/log.h"
#include "acevo/ui/cohtml_hooks.h"
#include "acevo/ui/menu_refresh_fix.h"
#include "acevo/ui/restyle_fix.h"
#include "acevo/ui/style_match_fix.h"

// The responsive UI, from the UI lag work of 2026-09-13 to 2026-09-15 (BUG-014 and the bugs it split
// into). It is made of these parts, each in its own file or section.
//   the stylesheet the overlay serves, with the generic hover and focus selector parts narrowed
//   the restyle fix, a hover no longer restyles every element after the hovered one
//   the menu refresh fix, the menu and the HUD in a session update every frame
//   the style matching fix, elements skip the rules they cannot match
//   the page fixes below, a script added to the menu and HUD view that runs before each page's own
//   the resource work move below, stylesheet parses no longer hold the frame

// The page fixes, tested outside the game against stand ins for the bundle before they went in.
static const char kPageFixesScript[] = R"js(
(function () {
    'use strict';
    if (window.__acevoUiFixes) return;

    // What the fixes did, read and reset once a second by the developer UI probe when it runs.
    var fixes = window.__acevoUiFixes = {
        navigation: { calls: 0, scans: 0, skipped: 0 },
        setup: { inits: 0, ignored: 0 },
        controls: { refreshes: 0, held: 0 }
    };

    // Registered before any page script, so it runs first in every animation frame and the page's
    // callbacks see the number of the frame they run in.
    var frame = 0;
    (function countFrames() {
        frame++;
        requestAnimationFrame(countFrames);
    })();

    // BUG-025. Every navigable element's setupNavigation calls makeFocusable() with no section,
    // which scans the whole page once per section. A group of the controls page makes a hundred
    // such calls in one frame while its rows are still outside the document, so every scan finds
    // the same elements. The first call of a frame scans and the rest fold into one scan on the
    // next frame, which also catches elements added later in that frame.
    function patchNavigation(navigation) {
        if (!navigation || navigation.__acevoPatched || typeof navigation.makeFocusable !== 'function') return;
        var stock = navigation.makeFocusable;
        var scannedFrame = -1;
        var trailing = false;

        navigation.makeFocusable = function (sectionId) {
            if (sectionId) return stock.apply(this, arguments);
            fixes.navigation.calls++;
            if (scannedFrame !== frame) {
                scannedFrame = frame;
                fixes.navigation.scans++;
                return stock.apply(this, arguments);
            }
            fixes.navigation.skipped++;
            if (trailing) return;
            trailing = true;
            var self = this;
            requestAnimationFrame(function () {
                trailing = false;
                try {
                    fixes.navigation.scans++;
                    stock.call(self);
                } catch (e) {
                    console.error('[ACEvoPerf] responsive ui trailing navigation scan failed', e);
                }
            });
        };
        navigation.__acevoPatched = true;
    }

    var navigationObject;
    try {
        Object.defineProperty(window, 'SpatialNavigation', {
            configurable: true,
            enumerable: true,
            get: function () { return navigationObject; },
            set: function (value) {
                navigationObject = value;
                try {
                    patchNavigation(value);
                } catch (e) {
                    console.error('[ACEvoPerf] responsive ui navigation patch failed', e);
                }
            }
        });
    } catch (e) {
        console.error('[ACEvoPerf] responsive ui could not watch SpatialNavigation', e);
    }

    // BUG-026. The vehicle setup page sends its Init request twice a few milliseconds apart and
    // rebuilds everything for each answer. A second init on the same element while its own request
    // is outstanding is ignored, the pending mark clears when the answer arrives or after three
    // seconds. Calls on different elements are never ignored.
    function patchVehicleSetup(proto) {
        if (!proto || proto.__acevoPatched || typeof proto.init !== 'function') return;
        var stockInit = proto.init;
        proto.init = function () {
            fixes.setup.inits++;
            var now = Date.now();
            if (this.__acevoInitPendingSince && now - this.__acevoInitPendingSince < 3000) {
                fixes.setup.ignored++;
                return;
            }
            var client = this.Client;
            var stockRequest = client && client.request;
            if (typeof stockRequest !== 'function') return stockInit.apply(this, arguments);

            var self = this;
            self.__acevoInitPendingSince = now;
            client.request = function (name, callback) {
                client.request = stockRequest;
                if (name !== 'Init' || typeof callback !== 'function') return stockRequest.apply(client, arguments);
                return stockRequest.call(client, name, function () {
                    self.__acevoInitPendingSince = 0;
                    return callback.apply(this, arguments);
                });
            };
            try {
                return stockInit.apply(this, arguments);
            } finally {
                if (client.request !== stockRequest) client.request = stockRequest;
            }
        };
        proto.__acevoPatched = true;
    }

    // The controls page. While one of its sliders is dragged the game answers every step with a full
    // refresh (InputConfigurationResponseRefresh with is_soft_set) 24 to 34 times a second, and each
    // one rebuilds the binding rows and resyncs every slider on the page. A soft refresh now runs at
    // most once every 100 ms, the newest one waiting in between and running when the time is up, so
    // the last step of a drag always lands. A refresh that is replaced still merges its settings the
    // way the page's handler starts, so the page ends where it would have. Other refreshes run at once.
    var kControlsRefreshMs = 100;

    function patchControlsRefresh(proto) {
        if (!proto || proto.__acevoPatched || typeof proto.onDevicesChanged !== 'function') return;
        var stockRefresh = proto.onDevicesChanged;

        function mergeReplaced(self, data) {
            if (data && data.wrapper && self.incomingSettings) Object.assign(self.incomingSettings, data.wrapper);
        }

        function run(self, data) {
            self.__acevoRefreshAt = Date.now();
            fixes.controls.refreshes++;
            return stockRefresh.call(self, data);
        }

        proto.onDevicesChanged = function (data) {
            var self = this;
            if (self.__acevoRefreshWaiting) {
                mergeReplaced(self, self.__acevoRefreshWaiting);
                self.__acevoRefreshWaiting = null;
            }
            var now = Date.now();
            var since = now - (self.__acevoRefreshAt || 0);
            if (!data || !data.is_soft_set || since >= kControlsRefreshMs) return run(self, data);

            fixes.controls.held++;
            self.__acevoRefreshWaiting = data;
            if (self.__acevoRefreshTimer) return;
            self.__acevoRefreshTimer = setTimeout(function () {
                self.__acevoRefreshTimer = 0;
                var waiting = self.__acevoRefreshWaiting;
                self.__acevoRefreshWaiting = null;
                if (!waiting) return;
                try {
                    run(self, waiting);
                } catch (e) {
                    console.error('[ACEvoPerf] responsive ui controls refresh failed', e);
                }
            }, kControlsRefreshMs - since);
        };
        proto.__acevoPatched = true;
    }

    try {
        var stockDefine = customElements.define;
        customElements.define = function (name, elementClass, options) {
            if (name === 'ks-page-vehiclesetup') {
                try {
                    patchVehicleSetup(elementClass && elementClass.prototype);
                } catch (e) {
                    console.error('[ACEvoPerf] responsive ui vehicle setup patch failed', e);
                }
            }
            if (name === 'ks-page-settings-controls') {
                try {
                    patchControlsRefresh(elementClass && elementClass.prototype);
                } catch (e) {
                    console.error('[ACEvoPerf] responsive ui controls refresh patch failed', e);
                }
            }
            return stockDefine.call(customElements, name, elementClass, options);
        };
    } catch (e) {
        console.error('[ACEvoPerf] responsive ui could not watch custom elements', e);
    }
})();
)js";

typedef void (*PFN_AddInitialScript)(void* view, const char* script);

static void OnView(void* view, int number, unsigned, unsigned)
{
    if (number != 1) return;
    auto addInitialScript = (PFN_AddInitialScript)(*(void***)view)[cohtml_slot::kViewAddInitialScript];
    addInitialScript(view, kPageFixesScript);
    Log("[responsive ui] page fixes added to the menu and HUD view");
}

// ---------------------------------------------------------------------------
// The resource work move
// ---------------------------------------------------------------------------

// Cohtml hands out its work through a callback the game answers by posting a job per notification:
// resources (type 0, loading, stylesheet parsing, image decoding) and style and layout (type 1). The
// game's frame thread runs queued jobs while its UI frame end waits (0xDE75E1), so on some page loads it
// picks up the 1.1 MB stylesheet's parse and the frame stalls for 30 to 60 ms, measured on 2026-09-15.
// A resource job has nothing the frame waits for, so when the frame thread takes one it is handed to a
// thread of the mod, which runs the same call. Style and layout work stays where it is, the frame needs
// its result. Before the game stops Cohtml's workers the handed over calls run on the stopping thread, so
// nothing reaches the library after it is gone.

static const uint64_t kResourceWork = 0;
static const int kMaxMoved = 64;

typedef uint64_t (*PFN_ExecuteWork)(void* library, uint64_t type, uint64_t mode, uint64_t family);
typedef void (*PFN_StopWorkers)(void* library);
typedef void (*PFN_Uninitialize)(void* library, uint64_t arg);

struct MovedWork {
    void* library;
    uint64_t mode;
    uint64_t family;
};

static PFN_ExecuteWork g_origExecuteWork = nullptr;
static PFN_StopWorkers g_origStopWorkers = nullptr;
static PFN_Uninitialize g_origUninitialize = nullptr;
static std::atomic<DWORD> g_frameThread{0};
static HANDLE g_movedSignal = nullptr;

static SRWLOCK g_movedLock = SRWLOCK_INIT;   // guards the queue, the stop flag and the running count
static MovedWork g_moved[kMaxMoved];
static int g_movedHead = 0;
static int g_movedCount = 0;
static int g_movedRunning = 0;
static bool g_movingStopped = false;
static CONDITION_VARIABLE g_movedIdle = CONDITION_VARIABLE_INIT;
static std::atomic<uint32_t> g_movedCalls{0};

static void OnFrameEnd(bool after)
{
    if (!after) g_frameThread.store(GetCurrentThreadId(), std::memory_order_relaxed);
}

static bool MoveWork(void* library, uint64_t mode, uint64_t family)
{
    AcquireSRWLockExclusive(&g_movedLock);
    bool moved = !g_movingStopped && g_movedCount < kMaxMoved;
    if (moved) {
        g_moved[(g_movedHead + g_movedCount) % kMaxMoved] = { library, mode, family };
        g_movedCount++;
    }
    ReleaseSRWLockExclusive(&g_movedLock);
    if (moved) {
        ReleaseSemaphore(g_movedSignal, 1, nullptr);
        g_movedCalls.fetch_add(1, std::memory_order_relaxed);
    }
    return moved;
}

static uint64_t Hook_ExecuteWork(void* library, uint64_t type, uint64_t mode, uint64_t family)
{
    if (type == kResourceWork && GetCurrentThreadId() == g_frameThread.load(std::memory_order_relaxed) && MoveWork(library, mode, family))
        return 0;
    return g_origExecuteWork(library, type, mode, family);
}

// Calls through the library's vtable, so the developer probe's timing sees the work run here.
static DWORD WINAPI MovedWorkThread(void*)
{
    SetThreadDescription(GetCurrentThread(), L"ACEvoPerf UI resource work");
    for (;;) {
        WaitForSingleObject(g_movedSignal, INFINITE);
        AcquireSRWLockExclusive(&g_movedLock);
        if (g_movingStopped || !g_movedCount) {
            ReleaseSRWLockExclusive(&g_movedLock);
            continue;
        }
        MovedWork work = g_moved[g_movedHead];
        g_movedHead = (g_movedHead + 1) % kMaxMoved;
        g_movedCount--;
        g_movedRunning++;
        ReleaseSRWLockExclusive(&g_movedLock);

        auto executeWork = (PFN_ExecuteWork)(*(void***)work.library)[cohtml_slot::kLibraryExecuteWork];
        executeWork(work.library, kResourceWork, work.mode, work.family);

        AcquireSRWLockExclusive(&g_movedLock);
        g_movedRunning--;
        ReleaseSRWLockExclusive(&g_movedLock);
        WakeAllConditionVariable(&g_movedIdle);
    }
}

static void StopMovingWork()
{
    MovedWork pending[kMaxMoved];
    int pendingCount = 0;
    AcquireSRWLockExclusive(&g_movedLock);
    g_movingStopped = true;
    for (; g_movedCount; g_movedCount--, g_movedHead = (g_movedHead + 1) % kMaxMoved) pending[pendingCount++] = g_moved[g_movedHead];
    while (g_movedRunning) SleepConditionVariableSRW(&g_movedIdle, &g_movedLock, INFINITE, 0);
    ReleaseSRWLockExclusive(&g_movedLock);
    for (int i = 0; i < pendingCount; ++i) g_origExecuteWork(pending[i].library, kResourceWork, pending[i].mode, pending[i].family);
}

static void Hook_StopWorkers(void* library)
{
    StopMovingWork();
    g_origStopWorkers(library);
}

static void Hook_Uninitialize(void* library, uint64_t arg)
{
    StopMovingWork();
    g_origUninitialize(library, arg);
}

static void OnLibrary(void* library)
{
    g_movedSignal = CreateSemaphoreW(nullptr, 0, kMaxMoved, nullptr);
    HANDLE thread = g_movedSignal ? CreateThread(nullptr, 0, &MovedWorkThread, nullptr, 0, nullptr) : nullptr;
    if (!thread) {
        Log("[responsive ui] could not start the resource work thread, the game runs its resource work as it does");
        return;
    }
    CloseHandle(thread);
    void** vtable = *(void***)library;
    HookVtableSlot(vtable, cohtml_slot::kLibraryStopWorkers, (void*)&Hook_StopWorkers, (void**)&g_origStopWorkers, "Cohtml Library::StopWorkers");
    HookVtableSlot(vtable, cohtml_slot::kLibraryUninitialize, (void*)&Hook_Uninitialize, (void**)&g_origUninitialize, "Cohtml Library::Uninitialize");
    HookVtableSlot(vtable, cohtml_slot::kLibraryExecuteWork, (void*)&Hook_ExecuteWork, (void**)&g_origExecuteWork, "Cohtml Library::ExecuteWork");
    Log("[responsive ui] resource work the game's frame thread picks up runs on the mod's thread");
}

void InstallResponsiveUi()
{
    if (!g_cfg.responsiveUi) return;
    InstallRestyleFix();
    InstallMenuRefreshFix();
    InstallStyleMatchFix();
    AddCohtmlViewListener(&OnView);
    AddCohtmlLibraryListener(&OnLibrary);
    AddUiFrameEndListener(&OnFrameEnd);
}

uint32_t ResponsiveUiTakeMovedWork()
{
    return g_movedCalls.exchange(0, std::memory_order_relaxed);
}
