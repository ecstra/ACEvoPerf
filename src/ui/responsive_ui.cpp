#include "acevo/ui/responsive_ui.h"
#include "acevo/core/config.h"
#include "acevo/core/iat.h"
#include "acevo/core/log.h"
#include "acevo/ui/child_removal_fix.h"
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

    // What the fixes did, read and reset once a second by the developer UI probe when it runs. Published
    // on window at the very end of this script, so that its presence means the whole script ran rather
    // than only that it started.
    var fixes = {
        navigation: { calls: 0, scans: 0, skipped: 0 },
        setup: { inits: 0, ignored: 0 },
        controls: { refreshes: 0, held: 0 }
    };

    // The same view carries the menus and the driving HUD, so this script is evaluated again for
    // hud.html at every session load. Nothing below can apply there. Neither page element exists, and the
    // navigation fold sees one call in a whole session, while the frame counter it needs would be a
    // permanent callback per frame on the page the driving frame rate is measured on. So nothing
    // installs here at all. Gating on the fold installing instead was tried and was useless, because
    // hud.html sets SpatialNavigation like every other page.
    if ((String(location.pathname || '').split('/').pop() || '') === 'hud.html') return;

    // Registered before any page script, so it runs first in every animation frame and the page's
    // callbacks see the number of the frame they run in. It has to stay at the top level to keep that:
    // animation frame callbacks run in the order they were registered and this one re-registers itself
    // as it runs, so wherever it first lands it stays, and anything that registered before it would then
    // read the previous frame's number.
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
            // A frame and nothing else. A timer alongside it was tried and taken out: Cohtml dispatches
            // timers out of the same view advance that drives frame callbacks, so when the view's clock
            // stands still, which is what happens when the window loses activation, a timer deadline
            // never comes due either. It could not fire in the one case it was added for, and on any
            // frame longer than its delay it fired first and claimed a frame number that had not been
            // incremented yet, which cost that frame the second whole page scan this fold exists to
            // remove. What actually recovers a frozen view is the first input event, which resumes the
            // frame callbacks too.
            requestAnimationFrame(function () {
                trailing = false;
                // Claim the frame this lands in. Without it a call arriving later in the same frame
                // sees the previous frame's number and takes the whole page scan path, so the frame
                // pays two scans.
                scannedFrame = frame;
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
            var sentInit = false;
            var wrapper = function (name, callback) {
                // Step aside only once the Init this is waiting for has actually come past. Stepping
                // aside on the first request of any name let an unrelated one that init sends first
                // take the wrapper off, after which the real Init reached the stock request, its answer
                // was never wrapped, and the mark stayed set for the full three seconds.
                if (name !== 'Init' || typeof callback !== 'function') return stockRequest.apply(client, arguments);
                sentInit = true;
                client.request = stockRequest;
                return stockRequest.call(client, name, function () {
                    self.__acevoInitPendingSince = 0;
                    return callback.apply(this, arguments);
                });
            };
            // The client belongs to the page, so it is entitled to refuse the write. Falling back beats
            // taking the page's own init down with us, and this is the one assignment here that reaches
            // an object the mod does not own.
            try {
                client.request = wrapper;
            } catch (e) {
                return stockInit.apply(this, arguments);
            }
            self.__acevoInitPendingSince = now;
            try {
                return stockInit.apply(this, arguments);
            } finally {
                // Take back only our own wrapper. Another element sharing this Client can have put its
                // own over ours in between, and restoring by comparing against the stock request would
                // reinstate a stale one.
                if (client.request === wrapper) client.request = stockRequest;
                // Track whether our Init went out rather than inferring it from the wrapper still being
                // installed. With a wrapper nested inside another those are different questions, and the
                // inference cleared a mark whose request was genuinely in flight. When no Init of ours
                // went out nothing will ever arrive to clear the mark, and leaving it set would make the
                // page's own second init, the one this exists to fold away, be ignored for three seconds
                // and a half built page stay on screen.
                if (!sentInit) self.__acevoInitPendingSince = 0;
            }
        };
        proto.__acevoPatched = true;
    }

    // The controls page. While one of its sliders is dragged the game answers every step with a full
    // refresh (InputConfigurationResponseRefresh with is_soft_set) 24 to 34 times a second, and each
    // one rebuilds the binding rows and resyncs every slider on the page. There are now at least 100 ms
    // between the end of one soft refresh and the start of the next, the newest one waiting in between
    // and running when the time is up, so the last step of a drag always lands. Measured from the end
    // rather than the start, so a rebuild costing more than the window does not make the next arrival
    // due the moment it returns. A refresh that is replaced still merges its settings the way the page's
    // handler starts, so the page ends where it would have. Other refreshes run at once.
    var kControlsRefreshMs = 100;

    function patchControlsRefresh(proto) {
        if (!proto || proto.__acevoPatched || typeof proto.onDevicesChanged !== 'function') return;
        var stockRefresh = proto.onDevicesChanged;

        function mergeReplaced(self, data) {
            if (data && data.wrapper && self.incomingSettings) Object.assign(self.incomingSettings, data.wrapper);
        }

        function run(self, data) {
            fixes.controls.refreshes++;
            try {
                return stockRefresh.call(self, data);
            } finally {
                // Stamped after, so the window is the gap between refreshes rather than the gap between
                // their starts. Stamped before, a rebuild costing more than the window made the next
                // arrival due the moment it returned, so refreshes ran back to back with no idle frame
                // and the held count read zero exactly when the page was slowest.
                self.__acevoRefreshAt = Date.now();
            }
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
                // A drag almost always ends with one refresh still held, and the player can leave the
                // page inside that window. Rebuilding every binding row then costs a frame on whatever
                // page they opened instead, and the stall reads as that page's rather than this one's.
                if (self.isConnected === false) return;
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

    // Last, so that anything reading this knows the script finished rather than merely started.
    window.__acevoUiFixes = fixes;
})();
)js";

typedef void (*PFN_AddInitialScript)(void* view, const char* script);

static void OnView(void* view, int, unsigned, unsigned, bool mainView)
{
    if (!mainView) return;
    auto addInitialScript = (PFN_AddInitialScript)(*(void***)view)[cohtml_slot::kViewAddInitialScript];
    addInitialScript(view, kPageFixesScript);
    // What is known here is that the script was handed to the view, not that it ran. Whether it ran to
    // the end is the presence of window.__acevoUiFixes, which only the developer probe can see.
    Log("[responsive ui] page fixes script given to the menu and HUD view");
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
// One thread and one semaphore for the process, not one per Cohtml library. Only ever written from
// OnLibrary, which runs on the thread that initialises the library, one library at a time.
static bool g_movedThreadUp = false;
// A moved call was given up on and the thread is still inside it. Guarded by g_movedLock.
static bool g_movedAbandoned = false;
static std::atomic<bool> g_movedQueueFullSaid{false};

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
    // Refuse while a call has been given up on, because the one thread that drains this queue is still
    // inside it. Accepting would return 0 for up to sixty four calls, each claiming work that nothing can
    // run, until the queue fills and the engine's own inline path takes over again.
    bool moved = !g_movingStopped && !g_movedAbandoned && g_movedCount < kMaxMoved;
    bool full = !g_movingStopped && !g_movedAbandoned && g_movedCount >= kMaxMoved;
    if (moved) {
        g_moved[(g_movedHead + g_movedCount) % kMaxMoved] = { library, mode, family };
        g_movedCount++;
        // Signalled under the lock so that a stop draining this entry always finds the count to take
        // back. Outside it, a stop landing between the push and the release drains the entry and leaves
        // its count behind.
        ReleaseSemaphore(g_movedSignal, 1, nullptr);
    }
    ReleaseSRWLockExclusive(&g_movedLock);
    if (moved) g_movedCalls.fetch_add(1, std::memory_order_relaxed);
    // A full queue is the one condition that hands the stall back to the frame thread mid session, and it
    // was invisible: the counter behind the probe's line counts calls taken, never calls refused. Said
    // once, because the frame thread is producing faster than the one consumer drains and saying it per
    // call would be its own cost.
    if (full && !g_movedQueueFullSaid.exchange(true, std::memory_order_relaxed))
        Log("[responsive ui] the moved resource work queue filled at %d, the game's frame thread runs its own resource work again until it drains", kMaxMoved);
    return moved;
}

static uint64_t Hook_ExecuteWork(void* library, uint64_t type, uint64_t mode, uint64_t family)
{
    if (type == kResourceWork && GetCurrentThreadId() == g_frameThread.load(std::memory_order_relaxed) && MoveWork(library, mode, family))
        return 0;
    return g_origExecuteWork(library, type, mode, family);
}

// Calls through the library's vtable, so the developer probe's timing sees the work run here. Guarded
// because a stop that gave up waiting lets the game tear the library down while this call is still in it,
// after which the vtable read or the call itself lands in freed memory. That fault belongs to a thread
// the game knows nothing about, so catching it here beats letting it take the process down.
static void RunMovedWork(const MovedWork& work)
{
    __try {
        auto executeWork = (PFN_ExecuteWork)(*(void***)work.library)[cohtml_slot::kLibraryExecuteWork];
        executeWork(work.library, kResourceWork, work.mode, work.family);
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        // Only the access violation, which is the freed library. Catching everything would swallow a
        // stack overflow too, and the logging below would then fault again on the unreset guard page.
        Log("[responsive ui] a moved resource work call faulted, the UI engine was most likely torn down while it was still in it");
    }
}

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
        // Arm the full queue line again once the burst has drained. Latched for the process it would
        // have reported the first fill and hidden every later one, which is the blindness it was added
        // to close.
        if (!g_movedCount) g_movedQueueFullSaid.store(false, std::memory_order_relaxed);
        g_movedRunning++;
        ReleaseSRWLockExclusive(&g_movedLock);

        RunMovedWork(work);

        AcquireSRWLockExclusive(&g_movedLock);
        g_movedRunning--;
        // A call that was given up on has come back, so this thread is healthy and later stops can wait
        // for it again. Leaving the flag set made one timeout permanent: every teardown after it would
        // skip the wait, destroy the library under a live call, and lean on the guard in RunMovedWork as
        // the normal path rather than the last resort. A thread that is genuinely wedged never reaches
        // this line, so the protection against paying the timeout twice at shutdown still holds.
        if (!g_movedRunning) g_movedAbandoned = false;
        ReleaseSRWLockExclusive(&g_movedLock);
        WakeAllConditionVariable(&g_movedIdle);
    }
}

static const DWORD kStopWaitMs = 5000;

// True when no moved call is still out, so the thread is parked on its semaphore holding nothing.
static bool StopMovingWork()
{
    MovedWork pending[kMaxMoved];
    int pendingCount = 0;
    bool gaveUpNow = false;
    AcquireSRWLockExclusive(&g_movedLock);
    g_movingStopped = true;
    for (; g_movedCount; g_movedCount--, g_movedHead = (g_movedHead + 1) % kMaxMoved) {
        pending[pendingCount++] = g_moved[g_movedHead];
        // Take back the count this entry put on the semaphore. Left behind, they wake the thread once per
        // drained entry after the next restart, and a full queue drained puts the semaphore on its
        // ceiling so the first release after that fails and an entry sits unsignalled.
        WaitForSingleObject(g_movedSignal, 0);
    }

    // Bounded rather than forever. This runs on the thread that is stopping Cohtml, which is the game's
    // frame thread, and the moved call can be inside Cohtml waiting on a job that only the frame thread
    // runs, so waiting without a limit is a deadlock neither side can break. The bound is measured against
    // a deadline rather than per sleep, because a spurious wake would otherwise start the five seconds
    // again and there would be no bound at all. Once a call has been given up on it is never waited for
    // again, since the thread is wedged and every later stop would pay the full time for nothing, which at
    // shutdown means paying it twice over.
    if (!g_movedAbandoned) {
        ULONGLONG deadline = GetTickCount64() + kStopWaitMs;
        while (g_movedRunning) {
            ULONGLONG now = GetTickCount64();
            if (now >= deadline) { gaveUpNow = true; g_movedAbandoned = true; break; }
            SleepConditionVariableSRW(&g_movedIdle, &g_movedLock, (DWORD)(deadline - now), 0);
        }
    }
    bool stillOut = g_movedAbandoned;
    ReleaseSRWLockExclusive(&g_movedLock);

    // The frame thread is learned again at the next frame end. Holding the old id across a stop would
    // match a thread that merely inherited it once the game's own frame thread had gone.
    g_frameThread.store(0, std::memory_order_relaxed);

    if (gaveUpNow)
        Log("[responsive ui] a moved resource work call did not finish within %u ms, going on without it", kStopWaitMs);
    if (pendingCount || stillOut)
        Log("[responsive ui] the UI engine is stopping, %d moved job(s) run here%s", pendingCount, stillOut ? ", one never came back" : "");
    for (int i = 0; i < pendingCount; ++i) g_origExecuteWork(pending[i].library, kResourceWork, pending[i].mode, pending[i].family);
    return !stillOut;
}

static void Hook_StopWorkers(void* library)
{
    StopMovingWork();
    g_origStopWorkers(library);
    // Stopping the workers is not the end of the library, Uninitialize is, and that runs its own stop
    // first. Latching the move off here left it off for the rest of the process whenever the game stopped
    // the workers and carried on using the library, which is F-05's fault on the sibling path.
    AcquireSRWLockExclusive(&g_movedLock);
    g_movingStopped = false;
    ReleaseSRWLockExclusive(&g_movedLock);
    // The stop drains the queue in its own loop, so the full queue line never re-arms on this path. A
    // queue that was full when the engine stopped would then stay silent for the rest of the process.
    g_movedQueueFullSaid.store(false, std::memory_order_relaxed);
}

static void Hook_Uninitialize(void* library, uint64_t arg)
{
    const bool stopped = StopMovingWork();
    // Said every time, since the stop's own lines appear only when work was left over or a call is still out,
    // and their absence was once read as the stop never running at exit.
    Log("[responsive ui] the UI engine is shutting down, %s",
        stopped ? "the moved resource work is stopped" : "with a moved resource work call still out");
    g_origUninitialize(library, arg);
}

static void OnLibrary(void* library)
{
    // The move only ever takes work off the frame thread, and OnFrameEnd is where that thread is learned,
    // so without the frame end hook g_frameThread stays 0, nothing is ever moved and the thread waits on a
    // semaphore nobody signals. The exe and the UI engine are checked separately, so a game update that
    // leaves Cohtml alone lands exactly here.
    if (!UiFrameEndHooked()) {
        Log("[responsive ui] the game UI's frame end is not followed on this build, so the frame thread is unknown and the resource work move stays out");
        return;
    }
    // Cohtml can be stopped and initialised again, and stopping latches the move off. Clear that here,
    // and reuse the thread and the semaphore already running. Making a second pair leaked both and left
    // MoveWork refusing every call for the rest of the process, while this function's last line still
    // reported the move as installed.
    AcquireSRWLockExclusive(&g_movedLock);
    g_movingStopped = false;
    ReleaseSRWLockExclusive(&g_movedLock);

    if (!g_movedThreadUp) {
        g_movedSignal = CreateSemaphoreW(nullptr, 0, kMaxMoved, nullptr);
        HANDLE thread = g_movedSignal ? CreateThread(nullptr, 0, &MovedWorkThread, nullptr, 0, nullptr) : nullptr;
        if (!thread) {
            if (g_movedSignal) { CloseHandle(g_movedSignal); g_movedSignal = nullptr; }
            Log("[responsive ui] could not start the resource work thread, the game runs its resource work as it does");
            return;
        }
        CloseHandle(thread);
        g_movedThreadUp = true;
    }
    void** vtable = *(void***)library;
    HookVtableSlot(vtable, cohtml_slot::kLibraryStopWorkers, (void*)&Hook_StopWorkers, (void**)&g_origStopWorkers, "Cohtml Library::StopWorkers");
    HookVtableSlot(vtable, cohtml_slot::kLibraryUninitialize, (void*)&Hook_Uninitialize, (void**)&g_origUninitialize, "Cohtml Library::Uninitialize");
    HookVtableSlot(vtable, cohtml_slot::kLibraryExecuteWork, (void*)&Hook_ExecuteWork, (void**)&g_origExecuteWork, "Cohtml Library::ExecuteWork");
    // HookVtableSlot is quiet when it cannot make the page writable, which is what a page protection
    // product would produce, so claim the move only once the one hook that does the work is really in.
    // All three, not just the one that moves the work. With the two stop hooks missing there is nothing
    // to drain the queue at teardown and a moved call can reach a library that has already gone, which
    // is worse than not moving at all.
    if (!g_origExecuteWork || !g_origStopWorkers || !g_origUninitialize) {
        // Latch the move off rather than only declining to claim it. The work hook may already be in the
        // vtable, and returning here would leave it moving work with nothing to drain the queue at
        // teardown, while this line says the move is out. Saying one thing and doing another is the
        // fault this whole branch keeps finding, so make the words true instead.
        AcquireSRWLockExclusive(&g_movedLock);
        g_movingStopped = true;
        ReleaseSRWLockExclusive(&g_movedLock);
        Log("[responsive ui] the UI engine's work calls could not all be wrapped, the game runs its resource work as it does");
        return;
    }
    Log("[responsive ui] resource work the game's frame thread picks up runs on the mod's thread");
}

void InstallResponsiveUi()
{
    if (!g_cfg.responsiveUi) return;
    InstallRestyleFix();
    InstallMenuRefreshFix();
    InstallStyleMatchFix();
    InstallChildRemovalFix();
    AddCohtmlViewListener(&OnView);
    AddCohtmlLibraryListener(&OnLibrary);
    AddUiFrameEndListener(&OnFrameEnd);
}

uint32_t ResponsiveUiTakeMovedWork()
{
    return g_movedCalls.exchange(0, std::memory_order_relaxed);
}
