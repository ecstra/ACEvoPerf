#include "acevo/ui/ui_probe.h"
#include "acevo/core/config.h"
#include "acevo/core/iat.h"
#include "acevo/core/log.h"

// Written against AssettoCorsaEVO.exe 0.9.1 (the Steam build of 2026-09-11) and
// cohtml.WindowsDesktop.dll 1.61.0.3, from the UI deep dive of 2026-09-14
// (.agent/docs/research/ui-lag-deepdive-2026-09-14.md).
//
// Cohtml is reached from the one export the exe imports, Library::Initialize, and then through
// vtables. Library slot 1 CreateSystem, slot 5 ExecuteWork(type, mode, family). System slot 3
// CreateView, ViewSettings width at +0x10 and height at +0x14. View slot 6 Advance(double ms),
// returning the frame id, and slot 61 AddInitialScript(const char*), which runs at the start of
// every document the view loads afterwards.
//
// The game's UI object (EvoUi) keeps its GameUi behind [this+0xA8]. Its slot 7 posts the frame's UI
// job (every listed view's Advance and the paint) and slot 8 waits for that job while running other
// queued jobs, which is how UI layout ends up on the render thread. Slot 8 passes the renderer on in
// rdx, so both wrappers pass every register argument through.
//
// GameUi::PostFrame lists every view each frame only while GameUi+0x555 (the main menu showroom) or
// +0x556 (pause) is set, otherwise one view a frame in rotation. The probe replaces that first test at
// 0xDE379B with a jump to a stub that also takes the every view branch while a flag of its own is set.
// The two game flags are left alone because other code reads them.

static const DWORD kTimeDateStamp = 0x6A9EC72A;
static const DWORD kSizeOfImage = 0x06CDD000;

static const uint32_t kRvaEvoUiVtable = 0x3173D58;
static const int kPostFrameSlot = 7;
static const int kEndFrameSlot = 8;
static const uint32_t kRvaPostFrameThunk = 0x0126E58;
static const uint32_t kRvaEndFrameThunk = 0x00EB461;

static const uint32_t kRvaRotationTest = 0x0DE379B;    // cmp byte [r14+0x555], 0 then jne, 14 bytes
static const uint32_t kRvaPauseTest = 0x0DE37A9;       // cmp byte [r14+0x556], 0
static const uint32_t kRvaEveryView = 0x0DE38B1;       // where both tests jump when set
static const size_t kRotationTestLength = 14;
static const size_t kRotationRegionLength = 0x1C;
static const uint64_t kRotationRegionHash = 0xB6AB78A1E9FA112Dull;

static const int kCreateSystemSlot = 1;
static const int kExecuteWorkSlot = 5;
static const int kCreateViewSlot = 3;
static const int kAdvanceSlot = 6;
static const int kAddInitialScriptSlot = 61;

static const int kEveryViewBlockSeconds = 20;

// The page script, tested outside the game against a stand in for the bundle before it went in.
static const char kPageScript[] = R"js(
(function () {
    'use strict';
    if (window.__acevoUiProbe) return;
    window.__acevoUiProbe = true;

    var page = String(location.pathname || '').split('/').pop() || 'unknown';

    // Fixes switch on every other visit of the same page, counted across document loads, so one
    // session carries its own control. The count is per page because pages are visited in
    // different orders.
    var visit = 1;
    try {
        var key = 'acevo_ui_probe_visit_' + page;
        visit = (parseInt(localStorage.getItem(key), 10) || 0) + 1;
        localStorage.setItem(key, String(visit));
    } catch (e) {}
    var fixesOn = visit % 2 === 0;
    var state = fixesOn ? 'on' : 'off';
    console.log('[ACEvoPerf] ui probe ' + page + ' visit ' + visit + ' fixes ' + state);

    var frame = 0;
    var stats = { navCalls: 0, navScans: 0, navSkipped: 0, navMs: 0, setupInits: 0, setupIgnored: 0, setupInstances: 0 };

    function report() {
        if (!stats.navCalls && !stats.navScans && !stats.setupInits) return;
        var elements = document.getElementsByTagName('*').length;
        console.log('[ACEvoPerf] ui probe ' + page + ' fixes ' + state +
            ' | navigation calls ' + stats.navCalls + ' scans ' + stats.navScans + ' skipped ' + stats.navSkipped +
            ' scan ms ' + stats.navMs +
            ' | setup init ' + stats.setupInits + ' ignored ' + stats.setupIgnored + ' instances ' + stats.setupInstances +
            ' | elements ' + elements);
        for (var name in stats) stats[name] = 0;
    }

    // Registered before any page script, so it runs first in every frame and the frame number
    // is current when the page's callbacks run. performance.now() is frozen within a Cohtml frame.
    var lastReport = Date.now();
    (function countFrames() {
        frame++;
        var now = Date.now();
        if (now - lastReport >= 1000) {
            lastReport = now;
            try { report(); } catch (e) {}
        }
        requestAnimationFrame(countFrames);
    })();

    // BUG-025. Every navigable element's setupNavigation calls makeFocusable() with no section,
    // which scans the whole page once per section. A group of the controls page makes a hundred
    // such calls in one frame while its rows are still outside the document, so every scan finds
    // the same elements. With the fixes on, the first call of a frame scans and the rest fold into
    // one scan on the next frame, which also catches elements added later in that frame.
    function patchNavigation(navigation) {
        if (!navigation || navigation.__acevoPatched || typeof navigation.makeFocusable !== 'function') return;
        var stock = navigation.makeFocusable;
        var scannedFrame = -1;
        var trailing = false;

        function scan(self, args) {
            var started = Date.now();
            try {
                return stock.apply(self, args);
            } finally {
                stats.navScans++;
                stats.navMs += Date.now() - started;
            }
        }

        navigation.makeFocusable = function (sectionId) {
            if (sectionId) return stock.apply(this, arguments);
            stats.navCalls++;
            if (!fixesOn || scannedFrame !== frame) {
                scannedFrame = frame;
                return scan(this, arguments);
            }
            stats.navSkipped++;
            if (trailing) return;
            trailing = true;
            var self = this;
            requestAnimationFrame(function () {
                trailing = false;
                try {
                    scan(self, []);
                } catch (e) {
                    console.error('[ACEvoPerf] ui probe trailing navigation scan failed', e);
                }
            });
        };
        navigation.__acevoPatched = true;
        console.log('[ACEvoPerf] ui probe navigation scans patched on ' + page);
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
                    console.error('[ACEvoPerf] ui probe navigation patch failed', e);
                }
            }
        });
    } catch (e) {
        console.error('[ACEvoPerf] ui probe could not watch SpatialNavigation', e);
    }

    // BUG-026. The vehicle setup page sends its Init request twice a few milliseconds apart and
    // rebuilds everything for each answer. With the fixes on, a second init on the same element
    // while its own request is outstanding is ignored, the pending mark clears when the answer
    // arrives or after three seconds. Calls on different elements are counted, never ignored.
    var setupInstanceCount = 0;

    function patchVehicleSetup(proto) {
        if (!proto || proto.__acevoPatched || typeof proto.init !== 'function') return;
        var stockInit = proto.init;
        proto.init = function () {
            stats.setupInits++;
            if (!this.__acevoSetupId) {
                this.__acevoSetupId = ++setupInstanceCount;
                stats.setupInstances++;
            }
            if (fixesOn && this.__acevoInitPendingSince && Date.now() - this.__acevoInitPendingSince < 3000) {
                stats.setupIgnored++;
                return;
            }
            var client = this.Client;
            var stockRequest = client && client.request;
            if (!fixesOn || typeof stockRequest !== 'function') return stockInit.apply(this, arguments);

            var self = this;
            self.__acevoInitPendingSince = Date.now();
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
        console.log('[ACEvoPerf] ui probe vehicle setup init patched on ' + page);
    }

    try {
        var stockDefine = customElements.define;
        customElements.define = function (name, elementClass, options) {
            if (name === 'ks-page-vehiclesetup') {
                try {
                    patchVehicleSetup(elementClass && elementClass.prototype);
                } catch (e) {
                    console.error('[ACEvoPerf] ui probe vehicle setup patch failed', e);
                }
            }
            return stockDefine.call(customElements, name, elementClass, options);
        };
    } catch (e) {
        console.error('[ACEvoPerf] ui probe could not watch custom elements', e);
    }
})();
)js";

typedef void* (*PFN_LibraryInitialize)(const char* licenseKey, const void* params);
typedef void* (*PFN_CreateSystem)(void* library, const void* settings);
typedef void* (*PFN_CreateView)(void* system, const void* settings);
typedef uint64_t (*PFN_ExecuteWork)(void* library, uint64_t type, uint64_t mode, uint64_t family);
typedef uint64_t (*PFN_Advance)(void* view, double milliseconds, uint64_t arg3, uint64_t arg4);
typedef void (*PFN_AddInitialScript)(void* view, const char* script);
typedef void (*PFN_UiFrame)(void* evoUi, void* arg2, void* arg3, void* arg4);

static PFN_LibraryInitialize g_origInitialize = nullptr;
static PFN_CreateSystem g_origCreateSystem = nullptr;
static PFN_CreateView g_origCreateView = nullptr;
static PFN_ExecuteWork g_origExecuteWork = nullptr;
static PFN_Advance g_origAdvance = nullptr;
static PFN_UiFrame g_origPostFrame = nullptr;
static PFN_UiFrame g_origEndFrame = nullptr;

static HMODULE g_cohtml = nullptr;
static LARGE_INTEGER g_qpf = {};
static BYTE* g_everyViewFlag = nullptr;

static std::atomic<uint64_t> g_endFrameUsSincePresent{0};
static std::atomic<uint64_t> g_advanceUsSincePresent{0};
static std::atomic<DWORD> g_postFrameThread{0};
static std::atomic<DWORD> g_endFrameThread{0};

static const int kMaxViews = 32;
static const int kWorkTypes = 3;    // 0 resource, 1 layout, anything else counted as 2

struct ViewStats {
    void* view;
    int number;
    unsigned width, height;
    uint32_t advances;
    uint64_t us, maxUs;
};

struct SecondStats {
    ViewStats views[kMaxViews];
    int viewCount;
    uint32_t workCalls[kWorkTypes];
    uint64_t workUs[kWorkTypes], workRenderUs[kWorkTypes], workMaxUs[kWorkTypes];
    uint32_t endFrames;
    uint64_t endFrameUs, endFrameMaxUs;
};

static SRWLOCK g_statsLock = SRWLOCK_INIT;
static SecondStats g_stats = {};
static int g_viewsCreated = 0;

static int64_t Qpc()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

static uint64_t ElapsedUs(int64_t since)
{
    return (uint64_t)((Qpc() - since) * 1000000 / g_qpf.QuadPart);
}

static uint64_t Fnv1a64(const BYTE* bytes, size_t length)
{
    uint64_t hash = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < length; ++i) {
        hash ^= bytes[i];
        hash *= 0x100000001B3ull;
    }
    return hash;
}

// Logs the first few thread changes only, in case the call moves between job threads every frame.
static void NoteThread(std::atomic<DWORD>& slot, const char* what)
{
    static std::atomic<int> changesLogged{0};
    DWORD thread = GetCurrentThreadId();
    DWORD previous = slot.exchange(thread, std::memory_order_relaxed);
    if (previous != thread && changesLogged.fetch_add(1, std::memory_order_relaxed) < 16)
        Log("[ui] %s runs on thread %lu", what, thread);
}

// ---------------------------------------------------------------------------
// Cohtml
// ---------------------------------------------------------------------------

static uint64_t Hook_Advance(void* view, double milliseconds, uint64_t arg3, uint64_t arg4)
{
    int64_t started = Qpc();
    uint64_t frameId = g_origAdvance(view, milliseconds, arg3, arg4);
    uint64_t us = ElapsedUs(started);
    g_advanceUsSincePresent.fetch_add(us, std::memory_order_relaxed);

    AcquireSRWLockExclusive(&g_statsLock);
    for (int i = 0; i < g_stats.viewCount; ++i) {
        ViewStats& stats = g_stats.views[i];
        if (stats.view != view) continue;
        stats.advances++;
        stats.us += us;
        stats.maxUs = std::max(stats.maxUs, us);
        break;
    }
    ReleaseSRWLockExclusive(&g_statsLock);
    return frameId;
}

static uint64_t Hook_ExecuteWork(void* library, uint64_t type, uint64_t mode, uint64_t family)
{
    int64_t started = Qpc();
    uint64_t result = g_origExecuteWork(library, type, mode, family);
    uint64_t us = ElapsedUs(started);
    int slot = (type < kWorkTypes - 1) ? (int)type : kWorkTypes - 1;
    bool onRenderThread = GetCurrentThreadId() == g_endFrameThread.load(std::memory_order_relaxed);

    AcquireSRWLockExclusive(&g_statsLock);
    g_stats.workCalls[slot]++;
    g_stats.workUs[slot] += us;
    if (onRenderThread) g_stats.workRenderUs[slot] += us;
    g_stats.workMaxUs[slot] = std::max(g_stats.workMaxUs[slot], us);
    ReleaseSRWLockExclusive(&g_statsLock);
    return result;
}

static void* Hook_CreateView(void* system, const void* settings)
{
    unsigned width = *(const unsigned*)((const BYTE*)settings + 0x10);
    unsigned height = *(const unsigned*)((const BYTE*)settings + 0x14);
    void* view = g_origCreateView(system, settings);
    if (!view) {
        Log("[ui] Cohtml view %ux%u was not created", width, height);
        return view;
    }

    int number = ++g_viewsCreated;
    AcquireSRWLockExclusive(&g_statsLock);
    // Car displays are created again at every session load, so a full table gives up its oldest display.
    int slot = -1;
    for (int i = 0; i < g_stats.viewCount; ++i) if (g_stats.views[i].view == view) slot = i;
    if (slot < 0 && g_stats.viewCount < kMaxViews) slot = g_stats.viewCount++;
    if (slot < 0) {
        for (int i = 0; i < g_stats.viewCount; ++i)
            if (g_stats.views[i].number > 1 && (slot < 0 || g_stats.views[i].number < g_stats.views[slot].number)) slot = i;
    }
    if (slot >= 0) {
        g_stats.views[slot] = {};
        g_stats.views[slot].view = view;
        g_stats.views[slot].number = number;
        g_stats.views[slot].width = width;
        g_stats.views[slot].height = height;
    }
    ReleaseSRWLockExclusive(&g_statsLock);

    void** vtable = *(void***)view;
    HookVtableSlot(vtable, kAdvanceSlot, (void*)&Hook_Advance, (void**)&g_origAdvance, "Cohtml View::Advance");
    Log("[ui] Cohtml view #%d %ux%u created", number, width, height);

    // The first view is the menu and HUD view, the car displays come after it and keep their scripts.
    if (number == 1) {
        auto addInitialScript = (PFN_AddInitialScript)vtable[kAddInitialScriptSlot];
        addInitialScript(view, kPageScript);
        Log("[ui] page script added to view #1, the page fixes switch on every other visit of a page");
    }
    return view;
}

static void* Hook_CreateSystem(void* library, const void* settings)
{
    void* system = g_origCreateSystem(library, settings);
    if (!system) {
        Log("[ui] Cohtml system was not created");
        return system;
    }
    HookVtableSlot(*(void***)system, kCreateViewSlot, (void*)&Hook_CreateView, (void**)&g_origCreateView, "Cohtml System::CreateView");
    return system;
}

static void* Hook_LibraryInitialize(const char* licenseKey, const void* params)
{
    void* library = g_origInitialize(licenseKey, params);
    if (!library) {
        Log("[ui] Cohtml library initialisation failed");
        return library;
    }
    void** vtable = *(void***)library;
    HookVtableSlot(vtable, kCreateSystemSlot, (void*)&Hook_CreateSystem, (void**)&g_origCreateSystem, "Cohtml Library::CreateSystem");
    HookVtableSlot(vtable, kExecuteWorkSlot, (void*)&Hook_ExecuteWork, (void**)&g_origExecuteWork, "Cohtml Library::ExecuteWork");
    return library;
}

static void InstallCohtmlHooks()
{
    g_cohtml = GetModuleHandleW(L"cohtml.WindowsDesktop.dll");
    void* initialize = g_cohtml ? (void*)GetProcAddress(g_cohtml, "?Initialize@Library@cohtml@@SAPEAV12@PEBDAEBULibraryParams@2@@Z") : nullptr;
    if (!initialize) {
        Log("[ui] Cohtml Library::Initialize not found, the Cohtml side of the probe is off");
        return;
    }
    g_origInitialize = (PFN_LibraryInitialize)initialize;
    int patched = PatchIatByAddress(GetModuleHandleW(nullptr), initialize, (void*)&Hook_LibraryInitialize);
    Log("[ui] Cohtml Library::Initialize, %d import slot(s) of the exe patched", patched);
}

// ---------------------------------------------------------------------------
// The game's UI frame
// ---------------------------------------------------------------------------

static void Hook_PostFrame(void* evoUi, void* arg2, void* arg3, void* arg4)
{
    NoteThread(g_postFrameThread, "the UI frame post");
    g_origPostFrame(evoUi, arg2, arg3, arg4);
}

static void Hook_EndFrame(void* evoUi, void* arg2, void* arg3, void* arg4)
{
    NoteThread(g_endFrameThread, "the UI frame end");
    int64_t started = Qpc();
    g_origEndFrame(evoUi, arg2, arg3, arg4);
    uint64_t us = ElapsedUs(started);
    g_endFrameUsSincePresent.fetch_add(us, std::memory_order_relaxed);

    AcquireSRWLockExclusive(&g_statsLock);
    g_stats.endFrames++;
    g_stats.endFrameUs += us;
    g_stats.endFrameMaxUs = std::max(g_stats.endFrameMaxUs, us);
    ReleaseSRWLockExclusive(&g_statsLock);
}

static bool ThisIsTheGameBuild(BYTE* base)
{
    auto nt = (IMAGE_NT_HEADERS64*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    return nt->FileHeader.TimeDateStamp == kTimeDateStamp && nt->OptionalHeader.SizeOfImage == kSizeOfImage;
}

static void InstallUiFrameHooks(BYTE* base)
{
    void** vtable = (void**)(base + kRvaEvoUiVtable);
    if ((BYTE*)vtable[kPostFrameSlot] != base + kRvaPostFrameThunk || (BYTE*)vtable[kEndFrameSlot] != base + kRvaEndFrameThunk) {
        Log("[ui] the game UI's vtable is not where it was read, frame post and end are not timed");
        return;
    }
    HookVtableSlot(vtable, kPostFrameSlot, (void*)&Hook_PostFrame, (void**)&g_origPostFrame, "the game UI frame post");
    HookVtableSlot(vtable, kEndFrameSlot, (void*)&Hook_EndFrame, (void**)&g_origEndFrame, "the game UI frame end");
}

// The stub takes the every view branch when the probe's flag is set, then runs the game's own test.
//   cmp byte [rip+flag], 0 / jne every / cmp byte [r14+0x555], 0 / jne every / jmp pause test
//   every: jmp every view branch
static void InstallEveryViewSwitch(BYTE* base)
{
    BYTE* site = base + kRvaRotationTest;
    if (Fnv1a64(site, kRotationRegionLength) != kRotationRegionHash) {
        Log("[ui] the UI view rotation test at rva 0x%07X is not the code this was written against, the every view switch is off", (unsigned)kRvaRotationTest);
        return;
    }

    const size_t page = 0x1000;
    BYTE* stub = (BYTE*)VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!stub) {
        Log("[ui] no memory for the every view stub, the switch is off");
        return;
    }
    g_everyViewFlag = stub + page;

    BYTE code[] = {
        0x80, 0x3D, 0, 0, 0, 0, 0x00,                       // cmp byte ptr [rip + flag], 0
        0x75, 0x18,                                         // jne every
        0x41, 0x80, 0xBE, 0x55, 0x05, 0x00, 0x00, 0x00,     // cmp byte ptr [r14 + 0x555], 0
        0x75, 0x0E,                                         // jne every
        0xFF, 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     // jmp qword ptr [rip], the pause test
        0xFF, 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     // every: jmp qword ptr [rip], the every view branch
    };
    int32_t flagDisp = (int32_t)(g_everyViewFlag - (stub + 7));
    memcpy(code + 2, &flagDisp, 4);
    BYTE* pauseTest = base + kRvaPauseTest;
    BYTE* everyView = base + kRvaEveryView;
    memcpy(code + 25, &pauseTest, 8);
    memcpy(code + 39, &everyView, 8);
    memcpy(stub, code, sizeof code);

    DWORD old = 0;
    if (!VirtualProtect(stub, page, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(stub, 0, MEM_RELEASE);
        g_everyViewFlag = nullptr;
        Log("[ui] could not make the every view stub executable, the switch is off");
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), stub, page);

    BYTE jump[kRotationTestLength] = { 0xFF, 0x25, 0, 0, 0, 0 };    // jmp qword ptr [rip], the stub
    memcpy(jump + 6, &stub, 8);
    if (!VirtualProtect(site, kRotationTestLength, PAGE_EXECUTE_READWRITE, &old)) {
        Log("[ui] could not make the UI view rotation test writable, the every view switch is off");
        g_everyViewFlag = nullptr;
        return;
    }
    memcpy(site, jump, sizeof jump);
    VirtualProtect(site, kRotationTestLength, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, kRotationTestLength);
    Log("[ui] every view switch installed at rva 0x%07X, it alternates in %d second blocks starting off", (unsigned)kRvaRotationTest, kEveryViewBlockSeconds);
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

void InstallUiProbe()
{
    if (!g_cfg.uiProbe) return;
    QueryPerformanceFrequency(&g_qpf);
    InstallCohtmlHooks();

    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    if (!ThisIsTheGameBuild(base)) {
        Log("[ui] this is not the game build the UI frame hooks were written for, only the Cohtml side of the probe runs");
        return;
    }
    InstallUiFrameHooks(base);
    InstallEveryViewSwitch(base);
}

uint32_t UiProbeTakeEndFrameUs()
{
    return (uint32_t)std::min<uint64_t>(g_endFrameUsSincePresent.exchange(0, std::memory_order_relaxed), UINT32_MAX);
}

uint32_t UiProbeTakeAdvanceUs()
{
    return (uint32_t)std::min<uint64_t>(g_advanceUsSincePresent.exchange(0, std::memory_order_relaxed), UINT32_MAX);
}

bool UiProbeEveryView()
{
    return g_everyViewFlag && *g_everyViewFlag;
}

void UiProbeTick()
{
    if (!g_cfg.uiProbe) return;

    static int seconds = 0;
    if (g_everyViewFlag && ++seconds % kEveryViewBlockSeconds == 0) {
        *g_everyViewFlag = !*g_everyViewFlag;
        Log("[ui] every view every frame %s", *g_everyViewFlag ? "on" : "off");
    }

    SecondStats second;
    AcquireSRWLockExclusive(&g_statsLock);
    second = g_stats;
    for (int i = 0; i < g_stats.viewCount; ++i) {
        ViewStats& stats = g_stats.views[i];
        stats.advances = 0;
        stats.us = stats.maxUs = 0;
    }
    memset(g_stats.workCalls, 0, sizeof g_stats.workCalls);
    memset(g_stats.workUs, 0, sizeof g_stats.workUs);
    memset(g_stats.workRenderUs, 0, sizeof g_stats.workRenderUs);
    memset(g_stats.workMaxUs, 0, sizeof g_stats.workMaxUs);
    g_stats.endFrames = 0;
    g_stats.endFrameUs = g_stats.endFrameMaxUs = 0;
    ReleaseSRWLockExclusive(&g_statsLock);

    char line[2048];
    int length = _snprintf_s(line, sizeof line, _TRUNCATE, "[ui] every view %s | end frame %u, %.1f ms, max %.1f ms",
        UiProbeEveryView() ? "on" : "off", second.endFrames, second.endFrameUs / 1000.0, second.endFrameMaxUs / 1000.0);
    bool advanced = false;
    for (int i = 0; i < second.viewCount && length > 0; ++i) {
        const ViewStats& stats = second.views[i];
        if (!stats.advances) continue;
        advanced = true;
        length += _snprintf_s(line + length, sizeof line - length, _TRUNCATE, " | view #%d %ux%u %u advances, %.1f ms, max %.1f ms",
            stats.number, stats.width, stats.height, stats.advances, stats.us / 1000.0, stats.maxUs / 1000.0);
    }
    static const char* kWorkNames[kWorkTypes] = { "resource", "layout", "other" };
    for (int t = 0; t < kWorkTypes && length > 0; ++t) {
        if (!second.workCalls[t]) continue;
        length += _snprintf_s(line + length, sizeof line - length, _TRUNCATE, " | %s work %u, %.1f ms, on the render thread %.1f ms, max %.1f ms",
            kWorkNames[t], second.workCalls[t], second.workUs[t] / 1000.0, second.workRenderUs[t] / 1000.0, second.workMaxUs[t] / 1000.0);
    }
    if (advanced || second.endFrames) Log("%s", line);
}
