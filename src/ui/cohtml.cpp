#include "acevo/ui/cohtml.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/core/iat.h"
#include "acevo/render/frame_stats.h"

// Layouts read from the exe's own set up code on 0.9.0 (the fields it writes before each call):
//   LibraryParams   0xb0 bytes, passed to Library::Initialize, logged raw
//   Library vtable  slot 1 CreateSystem(const SystemSettings&)
//                   slot 5 ExecuteWork(WorkType, WorkExecutionMode, count), the engine's layout
//                   and resource work, run by the game on the thread of its choice
//   SystemSettings  0x80 bytes: +0x68 int DebuggerPort (the game passes -1), +0x6c bool EnableDebugger
//   System vtable   slot 3 CreateView(const ViewSettings&)
//   ViewSettings    +0x10 unsigned Width, +0x14 unsigned Height
// The settings structs live on the game's stack for the call only, so a patched copy is safe.
static const size_t kLibraryParamsSize = 0xb0;
static const size_t kSystemSettingsSize = 0x80;
static const size_t kDebuggerPortOffset = 0x68;
static const size_t kEnableDebuggerOffset = 0x6c;
static const int kCreateSystemSlot = 1;
static const int kExecuteWorkSlot = 5;
static const int kCreateViewSlot = 3;
static const int kAddInitialScriptSlot = 61;   // View::AddInitialScript(const char*), runs at every new document

// The UI's own script, corrected where it hurts. Every lazily loaded component runs a per frame
// visibility loop that, every 125 ms on its own phase, reads its rectangle and moves its body
// into or out of the document. The game starts a fresh loop on every connect and every restore
// without ending the previous one, and with a hundred elements on their own phases every frame
// mixes a body move with the next element's read, so each read relayouts the whole page. The
// patch keeps one loop per element (a loop that finds its element already seen in this frame
// ends instead of rescheduling) and turns the checks into one sweep every 125 ms that reads
// every element first and moves the bodies after, one relayout per sweep. Cohtml gives every
// frame callback its own timestamp, so the frame is counted by a loop of our own that was
// registered first and therefore runs first.
static const char kUiPatch[] = R"js(
(function () {
    if (window.__acevoPatched) return;
    window.__acevoPatched = true;
    let frame = 0;
    let lastSweep = 0;
    const pending = new Set();
    function sweep() {
        const items = Array.from(pending);
        pending.clear();
        const visible = items.map((el) => el.isConnected && el.isOnScreen());
        items.forEach((el, i) => {
            if (!el.isConnected) return;
            const fragment = el.componentBodyFragment;
            if (visible[i]) {
                if ((fragment && fragment.children.length > 0) || el.dataset.fragment === 'true') el.restoreBodyFromFragment();
            } else if (fragment && fragment.children.length == 0) {
                el.moveBodyToFragment();
            }
        });
    }
    (function count() {
        frame++;
        const now = Date.now();
        try {
            if (now - lastSweep >= 125 && pending.size) { lastSweep = now; sweep(); }
        } catch (e) {
            console.error('[ACEvoPerf] visibility sweep failed', e);
        } finally {
            requestAnimationFrame(count);
        }
    })();
    const define = customElements.define.bind(customElements);
    let patched = false;
    customElements.define = function (name, cls, options) {
        if (!patched) {
            let proto = cls && cls.prototype;
            while (proto && !Object.prototype.hasOwnProperty.call(proto, 'checkOnScreenLoop')) proto = Object.getPrototypeOf(proto);
            if (proto) {
                proto.checkOnScreenLoop = function checkOnScreenLoop() {
                    if (this.__acevoFrame === frame) return;
                    this.__acevoFrame = frame;
                    pending.add(this);
                    this.lastVisibilityCheckFrameId = requestAnimationFrame(this.visibilityChecker);
                };
                patched = true;
                console.log('[ACEvoPerf] visibility loops batched into one sweep');
            }
        }
        return define(name, cls, options);
    };
})();
)js";

typedef void* (*PFN_LibraryInitialize)(const char* licenseKey, const void* params);
typedef void* (*PFN_CreateSystem)(void* library, const void* settings);
typedef void* (*PFN_ExecuteWork)(void* library, int workType, int mode, int count);
typedef void* (*PFN_CreateView)(void* system, const void* settings);
typedef void (*PFN_AddInitialScript)(void* view, const char* script);
typedef void (*PFN_V8SetFlags)(const char* flags);

typedef void (*PFN_OnWorkAvailable)(void* userData, int workType);

static PFN_LibraryInitialize g_origInitialize = nullptr;
static PFN_CreateSystem g_origCreateSystem = nullptr;
static PFN_ExecuteWork g_origExecuteWork = nullptr;
static PFN_CreateView g_origCreateView = nullptr;
static HMODULE g_cohtml = nullptr;
static int g_views = 0;

// The layout thread. Cohtml creates no threads of its own: it tells the game through the
// OnWorkAvailable callback in LibraryParams (+0x60, user data +0x68) that work of a type is
// pending and the game runs it through Library::ExecuteWork on whatever thread it likes. The
// game posts a job per notification, and its render thread executes queued jobs while it
// waits, so the UI's layout work (type 1, up to 50 ms a task) lands on the render thread. The
// mod takes the type 1 notifications and runs that work on a thread of its own instead.
static const size_t kOnWorkAvailableOffset = 0x60;
static const int kLayoutWork = 1;
static PFN_OnWorkAvailable g_gameOnWorkAvailable = nullptr;
static void* g_library = nullptr;
static HANDLE g_layoutEvent = nullptr;
static DWORD g_layoutThreadId = 0;
static void* Hook_ExecuteWork(void* library, int workType, int mode, int count);

static DWORD WINAPI LayoutThread(void*)
{
    typedef HRESULT (WINAPI *PFN_SetThreadDescription)(HANDLE, PCWSTR);
    auto describe = (PFN_SetThreadDescription)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription");
    if (describe) describe(GetCurrentThread(), L"ACEvoPerf Cohtml layout");
    for (;;) {
        WaitForSingleObject(g_layoutEvent, INFINITE);
        if (g_library && g_origExecuteWork) Hook_ExecuteWork(g_library, kLayoutWork, 0, -1);
    }
}

static void OnWorkAvailableProxy(void* userData, int workType)
{
    if (workType == kLayoutWork) { SetEvent(g_layoutEvent); return; }
    g_gameOnWorkAvailable(userData, workType);
}

// Work executed through Library::ExecuteWork, by calling thread and work type. Wall time against
// the thread's own CPU time tells executing from waiting.
static const int kWorkTypes = 4;
static const int kWorkThreads = 32;
struct WorkThread {
    DWORD tid;
    uint32_t calls[kWorkTypes], mode1[kWorkTypes];
    int maxCount[kWorkTypes];
    double ms[kWorkTypes], cpuMs[kWorkTypes], maxMs[kWorkTypes];
};
static WorkThread g_work[kWorkThreads];
static int g_workThreads = 0;
static CRITICAL_SECTION g_workCs;
static LARGE_INTEGER g_qpf;
static double g_cyclesPerMs = 0;

// Every distinct place in the exe that calls ExecuteWork, logged once with its arguments
static const int kWorkSites = 32;
struct WorkSite { uintptr_t rva; int type, mode, count; bool render; };
static WorkSite g_workSites[kWorkSites];
static int g_workSiteCount = 0;

static void NoteWorkSite(uintptr_t rva, int type, int mode, int count, bool render)
{
    for (int i = 0; i < g_workSiteCount; ++i)
        if (g_workSites[i].rva == rva && g_workSites[i].type == type && g_workSites[i].mode == mode && g_workSites[i].render == render) return;
    if (g_workSiteCount >= kWorkSites) return;
    g_workSites[g_workSiteCount++] = { rva, type, mode, count, render };
    Log("[ui] ExecuteWork site exe+0x%llX: type %d mode %d count %d on %s thread", (unsigned long long)rva, type, mode, count, render ? "the render" : "a job");
}

static uintptr_t VtableRva(void* object)
{
    return (uintptr_t)*(void**)object - (uintptr_t)g_cohtml;
}

static void* Hook_ExecuteWork(void* library, int workType, int mode, int count)
{
    LARGE_INTEGER t0, t1;
    ULONG64 c0 = 0, c1 = 0;
    QueryThreadCycleTime(GetCurrentThread(), &c0);
    QueryPerformanceCounter(&t0);
    void* result = g_origExecuteWork(library, workType, mode, count);
    QueryPerformanceCounter(&t1);
    QueryThreadCycleTime(GetCurrentThread(), &c1);
    double ms = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)g_qpf.QuadPart;
    double cpuMs = g_cyclesPerMs > 0 ? (double)(c1 - c0) / g_cyclesPerMs : 0;

    int type = (workType >= 0 && workType < kWorkTypes) ? workType : kWorkTypes - 1;
    DWORD tid = GetCurrentThreadId();
    bool render = tid == g_presentThreadId.load(std::memory_order_relaxed);
    EnterCriticalSection(&g_workCs);
    if (tid != g_layoutThreadId) NoteWorkSite((uintptr_t)_ReturnAddress() - (uintptr_t)GetModuleHandleW(nullptr), workType, mode, count, render);
    WorkThread* slot = nullptr;
    for (int i = 0; i < g_workThreads; ++i) if (g_work[i].tid == tid) { slot = &g_work[i]; break; }
    if (!slot && g_workThreads < kWorkThreads) { slot = &g_work[g_workThreads++]; slot->tid = tid; }
    if (slot) {
        slot->calls[type]++;
        slot->ms[type] += ms;
        slot->cpuMs[type] += cpuMs;
        if (ms > slot->maxMs[type]) slot->maxMs[type] = ms;
        if (mode == 1) slot->mode1[type]++;
        if (count > slot->maxCount[type]) slot->maxCount[type] = count;
    }
    LeaveCriticalSection(&g_workCs);
    return result;
}

void CohtmlWorkTick()
{
    if (!g_origExecuteWork) return;
    WorkThread snapshot[kWorkThreads];
    int n;
    EnterCriticalSection(&g_workCs);
    n = g_workThreads;
    memcpy(snapshot, g_work, sizeof(WorkThread) * n);
    for (int i = 0; i < n; ++i) { DWORD tid = g_work[i].tid; memset(&g_work[i], 0, sizeof g_work[i]); g_work[i].tid = tid; }
    LeaveCriticalSection(&g_workCs);

    char line[1024];
    int len = 0;
    uint32_t total = 0;
    DWORD render = g_presentThreadId.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) {
        uint32_t calls = 0;
        for (int t = 0; t < kWorkTypes; ++t) calls += snapshot[i].calls[t];
        if (!calls) continue;
        total += calls;
        bool isRender = snapshot[i].tid == render;
        const char* label = isRender ? "render " : snapshot[i].tid == g_layoutThreadId ? "layout " : "thread ";
        len += _snprintf_s(line + len, sizeof line - len, _TRUNCATE, " | %s%lu:", label, snapshot[i].tid);
        for (int t = 0; t < kWorkTypes; ++t) {
            const WorkThread& w = snapshot[i];
            if (!w.calls[t]) continue;
            if (isRender)
                len += _snprintf_s(line + len, sizeof line - len, _TRUNCATE, " type%d %u calls %.1f ms wall %.1f ms cpu max %.1f ms mode1 x%u count<=%d",
                    t, w.calls[t], w.ms[t], w.cpuMs[t], w.maxMs[t], w.mode1[t], w.maxCount[t]);
            else
                len += _snprintf_s(line + len, sizeof line - len, _TRUNCATE, " type%d %u calls %.1f ms", t, w.calls[t], w.ms[t]);
        }
    }
    if (total) Log("[ui] work%s", line);
}

static void* Hook_CreateView(void* system, const void* settings)
{
    auto bytes = (const uint8_t*)settings;
    unsigned width = *(const unsigned*)(bytes + 0x10);
    unsigned height = *(const unsigned*)(bytes + 0x14);
    void* view = g_origCreateView(system, settings);
    if (!view) { Log("[ui] Cohtml view %ux%u: creation failed", width, height); return view; }
    ++g_views;
    Log("[ui] Cohtml view #%d %ux%u created, vtable at cohtml+0x%llX", g_views, width, height, (unsigned long long)VtableRva(view));
    if (g_cfg.uiPatches && g_views == 1) {      // the menu and HUD view, the car displays keep their own scripts
        auto addInitialScript = (PFN_AddInitialScript)(*(void***)view)[kAddInitialScriptSlot];
        addInitialScript(view, kUiPatch);
        Log("[ui] UI patch script added to view #1, runs at every page start");
    }
    return view;
}

static void* Hook_CreateSystem(void* library, const void* settings)
{
    uint8_t copy[kSystemSettingsSize];
    memcpy(copy, settings, sizeof copy);
    int gamePort = *(int*)(copy + kDebuggerPortOffset);
    bool gameEnabled = copy[kEnableDebuggerOffset] != 0;
    if (g_cfg.uiInspectorPort > 0) {
        *(int*)(copy + kDebuggerPortOffset) = g_cfg.uiInspectorPort;
        copy[kEnableDebuggerOffset] = 1;
    }
    Log("[ui] Cohtml system: game asks inspector port %d enabled=%d, running with port %d enabled=%d",
        gamePort, gameEnabled, *(int*)(copy + kDebuggerPortOffset), copy[kEnableDebuggerOffset]);

    void* system = g_origCreateSystem(library, copy);
    if (!system) { Log("[ui] Cohtml system creation failed"); return system; }
    Log("[ui] Cohtml system created, vtable at cohtml+0x%llX", (unsigned long long)VtableRva(system));
    HookVtableSlot(*(void***)system, kCreateViewSlot, (void*)&Hook_CreateView, (void**)&g_origCreateView, "Cohtml System::CreateView");
    return system;
}

static void ApplyV8Flags()
{
    if (g_cfg.uiV8Flags.empty()) return;
    HMODULE v8 = GetModuleHandleW(L"v8.dll");
    auto setFlags = v8 ? (PFN_V8SetFlags)GetProcAddress(v8, "?SetFlagsFromString@V8@v8@@SAXPEBD@Z") : nullptr;
    if (!setFlags) { Log("[ui] v8.dll or V8::SetFlagsFromString not found, flags skipped"); return; }
    char flags[512];
    _snprintf_s(flags, sizeof flags, _TRUNCATE, "%ls", g_cfg.uiV8Flags.c_str());
    setFlags(flags);
    Log("[ui] V8 flags set before the script engine starts: %s", flags);
}

static void* Hook_LibraryInitialize(const char* licenseKey, const void* params)
{
    auto words = (const unsigned long long*)params;
    char line[kLibraryParamsSize / 8 * 17 + 1];
    int n = 0;
    for (size_t i = 0; i < kLibraryParamsSize / 8; ++i)
        n += _snprintf_s(line + n, sizeof line - n, _TRUNCATE, "%016llX ", words[i]);
    Log("[ui] Cohtml %s library params: %s", licenseKey ? "licensed" : "unlicensed", line);

    ApplyV8Flags();

    uint8_t copy[kLibraryParamsSize];
    memcpy(copy, params, sizeof copy);
    if (g_cfg.uiLayoutThread) {
        g_gameOnWorkAvailable = *(PFN_OnWorkAvailable*)(copy + kOnWorkAvailableOffset);
        g_layoutEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        HANDLE thread = g_gameOnWorkAvailable && g_layoutEvent ? CreateThread(nullptr, 0, &LayoutThread, nullptr, 0, &g_layoutThreadId) : nullptr;
        if (thread) {
            SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL);
            CloseHandle(thread);
            *(PFN_OnWorkAvailable*)(copy + kOnWorkAvailableOffset) = &OnWorkAvailableProxy;
            Log("[ui] layout thread %lu takes Cohtml's type %d work, the game's callback exe+0x%llX keeps the rest",
                g_layoutThreadId, kLayoutWork, (unsigned long long)((uintptr_t)g_gameOnWorkAvailable - (uintptr_t)GetModuleHandleW(nullptr)));
        } else {
            Log("[ui] layout thread not started, the game keeps all Cohtml work");
        }
    }

    void* library = g_origInitialize(licenseKey, copy);
    if (!library) { Log("[ui] Cohtml library initialisation failed"); return library; }
    Log("[ui] Cohtml library created, vtable at cohtml+0x%llX", (unsigned long long)VtableRva(library));
    void** vtable = *(void***)library;
    HookVtableSlot(vtable, kCreateSystemSlot, (void*)&Hook_CreateSystem, (void**)&g_origCreateSystem, "Cohtml Library::CreateSystem");
    HookVtableSlot(vtable, kExecuteWorkSlot, (void*)&Hook_ExecuteWork, (void**)&g_origExecuteWork, "Cohtml Library::ExecuteWork");
    g_library = library;
    return library;
}

void InstallCohtmlHooks()
{
    g_cohtml = GetModuleHandleW(L"cohtml.WindowsDesktop.dll");
    if (!g_cohtml) { Log("[ui] cohtml.WindowsDesktop.dll not loaded at attach time, hook skipped"); return; }
    void* real = (void*)GetProcAddress(g_cohtml, "?Initialize@Library@cohtml@@SAPEAV12@PEBDAEBULibraryParams@2@@Z");
    if (!real) { Log("[ui] Cohtml Library::Initialize export not found, hook skipped"); return; }
    InitializeCriticalSection(&g_workCs);
    QueryPerformanceFrequency(&g_qpf);
    DWORD mhz = 0, size = sizeof mhz;    // thread cycle times count at the processor's base clock
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"~MHz", RRF_RT_REG_DWORD, nullptr, &mhz, &size) == ERROR_SUCCESS && mhz)
        g_cyclesPerMs = (double)mhz * 1000.0;
    g_origInitialize = (PFN_LibraryInitialize)real;
    int patched = PatchIatByAddress(GetModuleHandleW(nullptr), real, (void*)&Hook_LibraryInitialize);
    Log("[ui] Cohtml Library::Initialize: %d import slot(s) of the exe patched, inspector port %d, v8 flags '%ls'",
        patched, g_cfg.uiInspectorPort, g_cfg.uiV8Flags.c_str());
}
