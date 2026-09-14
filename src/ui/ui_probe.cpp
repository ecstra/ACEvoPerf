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
// job (every listed view's Advance and the paint) with the UI clock as a float in xmm1 and a pointer
// in r8. Its slot 8 waits for that job while running other queued jobs, which is how UI layout ends up
// holding the render thread, and passes the renderer in rdx. Both wrappers pass every argument on.
//
// The layout sampler suspends a thread only while it is inside Cohtml's layout work, reads its stack
// with the unwind tables of the modules loaded at start, and resumes it before counting anything. It
// never allocates, locks or logs while a game thread is suspended.

static const DWORD kTimeDateStamp = 0x6A9EC72A;
static const DWORD kSizeOfImage = 0x06CDD000;

static const uint32_t kRvaEvoUiVtable = 0x3173D58;
static const int kPostFrameSlot = 7;
static const int kEndFrameSlot = 8;
static const uint32_t kRvaPostFrameThunk = 0x0126E58;
static const uint32_t kRvaEndFrameThunk = 0x00EB461;

static const int kCreateSystemSlot = 1;
static const int kExecuteWorkSlot = 5;
static const int kCreateViewSlot = 3;
static const int kAdvanceSlot = 6;
static const int kAddInitialScriptSlot = 61;
static const uint64_t kLayoutWork = 1;

// The page script, tested outside the game against a stand in for the bundle before it went in.
static const char kPageScript[] = R"js(
(function () {
    'use strict';
    if (window.__acevoUiProbe) return;
    window.__acevoUiProbe = true;

    var page = String(location.pathname || '').split('/').pop() || 'unknown';
    console.log('[ACEvoPerf] ui probe ' + page + ' loaded, page fixes on');

    var frame = 0;
    var framesThisSecond = 0;
    var nav = { calls: 0, scans: 0, skipped: 0 };
    var setup = { inits: 0, ignored: 0 };
    var changes = { classOps: 0, styleWrites: 0, attrWrites: 0, inserts: 0, removes: 0, htmlSets: 0, textSets: 0 };
    var events = { mousemove: 0, transitions: 0, animations: 0 };
    var top = {};
    var topKeys = 0;

    function describe(element) {
        if (!element || !element.tagName) return '?';
        var name = String(element.tagName).toLowerCase();
        if (element.id) return name + '#' + element.id;
        var list = element.classList;
        if (list && list.length) return name + '.' + list[0];
        return name;
    }

    // Counts one change to the page, keyed by what changed and on which element, so the log names
    // the scripts that make the page lay itself out again.
    function note(kind, element, detail) {
        changes[kind]++;
        var key = kind + ' ' + describe(element) + (detail ? ' ' + detail : '');
        if (top[key] === undefined) {
            if (topKeys >= 2000) return;
            topKeys++;
            top[key] = 0;
        }
        top[key]++;
    }

    function report() {
        var total = changes.classOps + changes.styleWrites + changes.attrWrites + changes.inserts + changes.removes + changes.htmlSets + changes.textSets;
        if (total || nav.calls || setup.inits) {
            var ranked = Object.keys(top).sort(function (a, b) { return top[b] - top[a]; }).slice(0, 10);
            console.log('[ACEvoPerf] ui changes ' + page + ' | frames ' + framesThisSecond +
                ' | class ' + changes.classOps + ' style ' + changes.styleWrites + ' attr ' + changes.attrWrites +
                ' insert ' + changes.inserts + ' remove ' + changes.removes + ' html ' + changes.htmlSets + ' text ' + changes.textSets +
                ' | mousemove ' + events.mousemove + ' transitions ' + events.transitions + ' animations ' + events.animations +
                ' | navigation calls ' + nav.calls + ' scans ' + nav.scans + ' skipped ' + nav.skipped +
                ' | setup init ' + setup.inits + ' ignored ' + setup.ignored +
                ' | top ' + ranked.map(function (key) { return key + ' x' + top[key]; }).join('; '));
        }
        for (var a in changes) changes[a] = 0;
        for (var b in events) events[b] = 0;
        nav.calls = nav.scans = nav.skipped = 0;
        setup.inits = setup.ignored = 0;
        top = {};
        topKeys = 0;
        framesThisSecond = 0;
    }

    // Registered before any page script, so it runs first in every frame and the frame number
    // is current when the page's callbacks run. performance.now() is frozen within a Cohtml frame.
    var lastReport = Date.now();
    (function countFrames() {
        frame++;
        framesThisSecond++;
        var now = Date.now();
        if (now - lastReport >= 1000) {
            lastReport = now;
            try { report(); } catch (e) {}
        }
        requestAnimationFrame(countFrames);
    })();

    function findDescriptor(object, property) {
        for (var proto = object; proto; proto = Object.getPrototypeOf(proto)) {
            var descriptor = Object.getOwnPropertyDescriptor(proto, property);
            if (descriptor) return { owner: proto, descriptor: descriptor };
        }
        return null;
    }

    var wrapped = [];

    function wrapMethod(proto, name, before) {
        if (!proto || typeof proto[name] !== 'function' || proto[name].__acevoWrapped) return;
        var stock = proto[name];
        var wrapper = function () {
            try { before(this, arguments); } catch (e) {}
            return stock.apply(this, arguments);
        };
        wrapper.__acevoWrapped = true;
        proto[name] = wrapper;
        wrapped.push(name);
    }

    function wrapSetter(proto, name, before) {
        var found = proto && findDescriptor(proto, name);
        if (!found || typeof found.descriptor.set !== 'function' || !found.descriptor.configurable) return;
        var stockSet = found.descriptor.set;
        Object.defineProperty(found.owner, name, {
            configurable: true,
            enumerable: found.descriptor.enumerable,
            get: found.descriptor.get,
            set: function (value) {
                try { before(this, value); } catch (e) {}
                return stockSet.call(this, value);
            }
        });
        wrapped.push(name + '=');
    }

    // Remembers which element a style or class list object belongs to, so their writes can be
    // attributed. The getter is wrapped only if the engine lets it be.
    function tagOwner(name) {
        var probe = document.createElement('div');
        var found = findDescriptor(probe, name);
        if (!found || typeof found.descriptor.get !== 'function' || !found.descriptor.configurable) return;
        var stockGet = found.descriptor.get;
        Object.defineProperty(found.owner, name, {
            configurable: true,
            enumerable: found.descriptor.enumerable,
            set: found.descriptor.set,
            get: function () {
                var value = stockGet.call(this);
                if (value && value.__acevoOwner !== this) {
                    try { value.__acevoOwner = this; } catch (e) {}
                }
                return value;
            }
        });
        wrapped.push(name + ' owner');
    }

    function installChangeCounters() {
        tagOwner('style');
        tagOwner('classList');

        var element = window.Element && Element.prototype;
        var node = window.Node && Node.prototype;
        wrapMethod(element, 'setAttribute', function (self, args) { note('attrWrites', self, String(args[0])); });
        wrapMethod(element, 'removeAttribute', function (self, args) { note('attrWrites', self, '-' + String(args[0])); });
        wrapSetter(element, 'className', function (self) { note('classOps', self, 'className'); });
        wrapSetter(element, 'innerHTML', function (self) { note('htmlSets', self, ''); });
        wrapSetter(node, 'textContent', function (self) { note('textSets', self, ''); });
        wrapMethod(node, 'appendChild', function (self, args) { note('inserts', self, describe(args[0])); });
        wrapMethod(node, 'insertBefore', function (self, args) { note('inserts', self, describe(args[0])); });
        wrapMethod(node, 'removeChild', function (self, args) { note('removes', self, describe(args[0])); });
        wrapMethod(node, 'replaceChild', function (self, args) { note('inserts', self, describe(args[0])); });

        var tokens = window.DOMTokenList && DOMTokenList.prototype;
        ['add', 'remove', 'toggle', 'replace'].forEach(function (name) {
            wrapMethod(tokens, name, function (self, args) {
                note('classOps', self.__acevoOwner, name + ' ' + Array.prototype.slice.call(args, 0, 2).join(' '));
            });
        });

        var style = window.CSSStyleDeclaration && CSSStyleDeclaration.prototype;
        wrapMethod(style, 'setProperty', function (self, args) { note('styleWrites', self.__acevoOwner, String(args[0])); });
        if (style) {
            Object.getOwnPropertyNames(style).forEach(function (name) {
                var descriptor = Object.getOwnPropertyDescriptor(style, name);
                if (!descriptor || typeof descriptor.set !== 'function' || !descriptor.configurable) return;
                var stockSet = descriptor.set;
                Object.defineProperty(style, name, {
                    configurable: true,
                    enumerable: descriptor.enumerable,
                    get: descriptor.get,
                    set: function (value) {
                        try { note('styleWrites', this.__acevoOwner, name); } catch (e) {}
                        return stockSet.call(this, value);
                    }
                });
            });
        }

        document.addEventListener('mousemove', function () { events.mousemove++; }, true);
        document.addEventListener('transitionstart', function () { events.transitions++; }, true);
        document.addEventListener('animationstart', function () { events.animations++; }, true);
        console.log('[ACEvoPerf] ui probe change counters on ' + page + ': ' + wrapped.join(', '));
    }

    try {
        installChangeCounters();
    } catch (e) {
        console.error('[ACEvoPerf] ui probe change counters failed', e);
    }

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
            nav.calls++;
            if (scannedFrame !== frame) {
                scannedFrame = frame;
                nav.scans++;
                return stock.apply(this, arguments);
            }
            nav.skipped++;
            if (trailing) return;
            trailing = true;
            var self = this;
            requestAnimationFrame(function () {
                trailing = false;
                try {
                    nav.scans++;
                    stock.call(self);
                } catch (e) {
                    console.error('[ACEvoPerf] ui probe trailing navigation scan failed', e);
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
                    console.error('[ACEvoPerf] ui probe navigation patch failed', e);
                }
            }
        });
    } catch (e) {
        console.error('[ACEvoPerf] ui probe could not watch SpatialNavigation', e);
    }

    // BUG-026. The vehicle setup page sends its Init request twice a few milliseconds apart and
    // rebuilds everything for each answer. A second init on the same element while its own request
    // is outstanding is ignored, the pending mark clears when the answer arrives or after three
    // seconds. Calls on different elements are never ignored.
    function patchVehicleSetup(proto) {
        if (!proto || proto.__acevoPatched || typeof proto.init !== 'function') return;
        var stockInit = proto.init;
        proto.init = function () {
            setup.inits++;
            var now = Date.now();
            if (this.__acevoInitPendingSince && now - this.__acevoInitPendingSince < 3000) {
                setup.ignored++;
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
typedef void (*PFN_PostFrame)(void* evoUi, float uiClock, void* arg3, void* arg4);
typedef void (*PFN_EndFrame)(void* evoUi, void* renderer, void* arg3, void* arg4);

static PFN_LibraryInitialize g_origInitialize = nullptr;
static PFN_CreateSystem g_origCreateSystem = nullptr;
static PFN_CreateView g_origCreateView = nullptr;
static PFN_ExecuteWork g_origExecuteWork = nullptr;
static PFN_Advance g_origAdvance = nullptr;
static PFN_PostFrame g_origPostFrame = nullptr;
static PFN_EndFrame g_origEndFrame = nullptr;

static LARGE_INTEGER g_qpf = {};
static void* g_mainView = nullptr;

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

// A clock the game hands Cohtml, followed over one second against real time.
struct ClockStats {
    uint32_t calls;
    double first, last, largestStep;
    int64_t firstQpc, lastQpc;
    uint32_t backwards;
};

struct SecondStats {
    ViewStats views[kMaxViews];
    int viewCount;
    uint32_t workCalls[kWorkTypes];
    uint64_t workUs[kWorkTypes], workRenderUs[kWorkTypes], workMaxUs[kWorkTypes];
    uint32_t endFrames;
    uint64_t endFrameUs, endFrameMaxUs;
    ClockStats postClock, advanceClock;
};

static SRWLOCK g_statsLock = SRWLOCK_INIT;
static SecondStats g_stats = {};
static double g_lastPostClock = 0, g_lastAdvanceClock = 0;
static bool g_postClockSeen = false, g_advanceClockSeen = false;
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

// Logs the first few thread changes only, in case the call moves between job threads every frame.
static void NoteThread(std::atomic<DWORD>& slot, const char* what)
{
    static std::atomic<int> changesLogged{0};
    DWORD thread = GetCurrentThreadId();
    DWORD previous = slot.exchange(thread, std::memory_order_relaxed);
    if (previous != thread && changesLogged.fetch_add(1, std::memory_order_relaxed) < 16)
        Log("[ui] %s runs on thread %lu", what, thread);
}

// Called under g_statsLock.
static void NoteClock(ClockStats& stats, double& last, bool& seen, double value)
{
    int64_t now = Qpc();
    if (seen) {
        double step = value - last;
        if (step < 0) stats.backwards++;
        else stats.largestStep = std::max(stats.largestStep, step);
    }
    if (!stats.calls) {
        stats.first = value;
        stats.firstQpc = now;
    }
    stats.last = value;
    stats.lastQpc = now;
    stats.calls++;
    last = value;
    seen = true;
}

// ---------------------------------------------------------------------------
// The layout sampler
// ---------------------------------------------------------------------------

struct UnwindModule {
    uintptr_t base, end;
    const RUNTIME_FUNCTION* functions;
    DWORD count;
    const char* label;
};

struct LayoutThread {
    DWORD id;
    HANDLE handle;
    std::atomic<int> inside;
};

static const int kMaxUnwindModules = 8;
static const int kMaxLayoutThreads = 16;
static const int kMaxFrames = 48;
static const int kCountSlots = 8192;
static const int kReportSeconds = 5;

static UnwindModule g_unwindModules[kMaxUnwindModules];
static int g_unwindModuleCount = 0;
static LayoutThread g_layoutThreads[kMaxLayoutThreads];
static std::atomic<int> g_layoutThreadCount{0};
static SRWLOCK g_registerLock = SRWLOCK_INIT;
static thread_local int t_layoutSlot = -1;

// Keys are the module index in the top byte and the function's rva below it.
struct Count {
    uint64_t key;
    uint32_t samples;
};
static Count g_leafCounts[kCountSlots];
static Count g_inclusiveCounts[kCountSlots];
static uint32_t g_samples = 0, g_unattributed = 0;

static void AddUnwindModule(const wchar_t* name, const char* label)
{
    HMODULE module = GetModuleHandleW(name);
    if (!module || g_unwindModuleCount >= kMaxUnwindModules) return;
    auto nt = (IMAGE_NT_HEADERS64*)((BYTE*)module + ((IMAGE_DOS_HEADER*)module)->e_lfanew);
    const IMAGE_DATA_DIRECTORY& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (!directory.VirtualAddress || !directory.Size) return;
    UnwindModule& entry = g_unwindModules[g_unwindModuleCount++];
    entry.base = (uintptr_t)module;
    entry.end = entry.base + nt->OptionalHeader.SizeOfImage;
    entry.functions = (const RUNTIME_FUNCTION*)((BYTE*)module + directory.VirtualAddress);
    entry.count = directory.Size / sizeof(RUNTIME_FUNCTION);
    entry.label = label;
}

// The exception directory is sorted by start address, which is what lets the loader search it too.
static const RUNTIME_FUNCTION* FindFunction(uintptr_t address, int& moduleIndex)
{
    for (int m = 0; m < g_unwindModuleCount; ++m) {
        const UnwindModule& module = g_unwindModules[m];
        if (address < module.base || address >= module.end) continue;
        moduleIndex = m;
        DWORD rva = (DWORD)(address - module.base);
        size_t low = 0, high = module.count;
        while (low < high) {
            size_t middle = (low + high) / 2;
            const RUNTIME_FUNCTION& function = module.functions[middle];
            if (rva < function.BeginAddress) high = middle;
            else if (rva >= function.EndAddress) low = middle + 1;
            else return &function;
        }
        return nullptr;
    }
    moduleIndex = -1;
    return nullptr;
}

static void AddCount(Count* table, uint64_t key)
{
    size_t slot = (size_t)((key * 0x9E3779B97F4A7C15ull) >> 51) & (kCountSlots - 1);
    for (int probe = 0; probe < kCountSlots; ++probe, slot = (slot + 1) & (kCountSlots - 1)) {
        if (table[slot].key == key) {
            table[slot].samples++;
            return;
        }
        if (!table[slot].key) {
            table[slot].key = key;
            table[slot].samples = 1;
            return;
        }
    }
}

static int FramesOf(HANDLE thread, uint64_t* keys)
{
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (!GetThreadContext(thread, &context)) return 0;

    int frames = 0;
    __try {
        while (frames < kMaxFrames && context.Rip) {
            int moduleIndex = -1;
            const RUNTIME_FUNCTION* function = FindFunction((uintptr_t)context.Rip, moduleIndex);
            if (moduleIndex < 0) break;    // code without unwind tables, script or another module
            const UnwindModule& module = g_unwindModules[moduleIndex];
            keys[frames++] = ((uint64_t)(moduleIndex + 1) << 56) | (function ? function->BeginAddress : (DWORD)(context.Rip - module.base));
            if (!function) {
                context.Rip = *(DWORD64*)context.Rsp;
                context.Rsp += 8;
                continue;
            }
            PVOID handlerData = nullptr;
            DWORD64 establisher = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, module.base, context.Rip, (PRUNTIME_FUNCTION)function, &context, &handlerData, &establisher, nullptr);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return frames;
}

static void TakeSample(LayoutThread& thread)
{
    uint64_t keys[kMaxFrames];
    int frames = 0;
    if (SuspendThread(thread.handle) == (DWORD)-1) return;
    if (thread.inside.load(std::memory_order_relaxed) > 0) frames = FramesOf(thread.handle, keys);
    ResumeThread(thread.handle);
    if (!frames) return;

    g_samples++;
    AddCount(g_leafCounts, keys[0]);
    for (int i = 0; i < frames; ++i) {
        bool seen = false;
        for (int j = 0; j < i && !seen; ++j) seen = keys[j] == keys[i];
        if (!seen) AddCount(g_inclusiveCounts, keys[i]);
    }
}

static int TopCounts(const Count* table, Count* top, int wanted)
{
    int found = 0;
    for (int slot = 0; slot < kCountSlots; ++slot) {
        if (!table[slot].key) continue;
        int at;
        if (found < wanted) {
            at = found++;
        } else if (table[slot].samples > top[wanted - 1].samples) {
            at = wanted - 1;
        } else {
            continue;
        }
        top[at] = table[slot];
        for (; at > 0 && top[at].samples > top[at - 1].samples; --at) std::swap(top[at], top[at - 1]);
    }
    return found;
}

static int AppendCounts(char* line, int length, size_t size, const char* title, const Count* table)
{
    Count top[12] = {};
    int found = TopCounts(table, top, 12);
    length += _snprintf_s(line + length, size - length, _TRUNCATE, " | %s", title);
    for (int i = 0; i < found && length > 0; ++i) {
        int moduleIndex = (int)(top[i].key >> 56) - 1;
        const char* label = moduleIndex >= 0 ? g_unwindModules[moduleIndex].label : "?";
        length += _snprintf_s(line + length, size - length, _TRUNCATE, " %s+0x%llX %.0f%%", label,
            (unsigned long long)(top[i].key & 0x00FFFFFFFFFFFFFFull), 100.0 * top[i].samples / std::max<uint32_t>(g_samples, 1));
    }
    return length;
}

static void ReportSamples()
{
    if (!g_samples) return;
    char line[3072];
    int length = _snprintf_s(line, sizeof line, _TRUNCATE, "[ui] layout samples %u", g_samples);
    length = AppendCounts(line, length, sizeof line, "innermost", g_leafCounts);
    if (length > 0) AppendCounts(line, length, sizeof line, "on the stack", g_inclusiveCounts);
    Log("%s", line);
    memset(g_leafCounts, 0, sizeof g_leafCounts);
    memset(g_inclusiveCounts, 0, sizeof g_inclusiveCounts);
    g_samples = 0;
}

static DWORD WINAPI LayoutSamplerThread(void*)
{
    SetThreadDescription(GetCurrentThread(), L"ACEvoPerf UI layout sampler");
    int64_t lastReport = Qpc();
    for (;;) {
        bool anyInside = false;
        int count = g_layoutThreadCount.load(std::memory_order_relaxed);
        for (int i = 0; i < count; ++i) {
            LayoutThread& thread = g_layoutThreads[i];
            if (thread.inside.load(std::memory_order_relaxed) <= 0) continue;
            anyInside = true;
            TakeSample(thread);
        }
        if (Qpc() - lastReport >= kReportSeconds * g_qpf.QuadPart) {
            lastReport = Qpc();
            ReportSamples();
        }
        Sleep(anyInside ? 1 : 4);
    }
}

static LayoutThread* LayoutThreadForThisThread()
{
    if (t_layoutSlot >= 0) return &g_layoutThreads[t_layoutSlot];
    AcquireSRWLockExclusive(&g_registerLock);
    int count = g_layoutThreadCount.load(std::memory_order_relaxed);
    if (count < kMaxLayoutThreads) {
        HANDLE handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
        if (handle) {
            g_layoutThreads[count].id = GetCurrentThreadId();
            g_layoutThreads[count].handle = handle;
            t_layoutSlot = count;
            g_layoutThreadCount.store(count + 1, std::memory_order_relaxed);
        }
    }
    ReleaseSRWLockExclusive(&g_registerLock);
    return t_layoutSlot >= 0 ? &g_layoutThreads[t_layoutSlot] : nullptr;
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
    if (view == g_mainView) NoteClock(g_stats.advanceClock, g_lastAdvanceClock, g_advanceClockSeen, milliseconds);
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
    LayoutThread* sampled = type == kLayoutWork ? LayoutThreadForThisThread() : nullptr;
    if (sampled) sampled->inside.fetch_add(1, std::memory_order_relaxed);
    int64_t started = Qpc();
    uint64_t result = g_origExecuteWork(library, type, mode, family);
    uint64_t us = ElapsedUs(started);
    if (sampled) sampled->inside.fetch_sub(1, std::memory_order_relaxed);

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
    if (number == 1) g_mainView = view;
    ReleaseSRWLockExclusive(&g_statsLock);

    void** vtable = *(void***)view;
    HookVtableSlot(vtable, kAdvanceSlot, (void*)&Hook_Advance, (void**)&g_origAdvance, "Cohtml View::Advance");
    Log("[ui] Cohtml view #%d %ux%u created", number, width, height);

    // The first view is the menu and HUD view, the car displays come after it and keep their scripts.
    if (number == 1) {
        auto addInitialScript = (PFN_AddInitialScript)vtable[kAddInitialScriptSlot];
        addInitialScript(view, kPageScript);
        Log("[ui] page script added to view #1, it counts page changes and applies the page fixes");
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
    HMODULE cohtml = GetModuleHandleW(L"cohtml.WindowsDesktop.dll");
    void* initialize = cohtml ? (void*)GetProcAddress(cohtml, "?Initialize@Library@cohtml@@SAPEAV12@PEBDAEBULibraryParams@2@@Z") : nullptr;
    if (!initialize) {
        Log("[ui] Cohtml Library::Initialize not found, the Cohtml side of the probe is off");
        return;
    }
    g_origInitialize = (PFN_LibraryInitialize)initialize;
    int patched = PatchIatByAddress(GetModuleHandleW(nullptr), initialize, (void*)&Hook_LibraryInitialize);
    Log("[ui] Cohtml Library::Initialize, %d import slot(s) of the exe patched", patched);

    AddUnwindModule(L"cohtml.WindowsDesktop.dll", "cohtml");
    AddUnwindModule(L"RenoirCore.WindowsDesktop.dll", "renoir");
    AddUnwindModule(L"v8.dll", "v8");
    AddUnwindModule(L"ucrtbase.dll", "ucrtbase");
    AddUnwindModule(L"ntdll.dll", "ntdll");
    AddUnwindModule(L"kernelbase.dll", "kernelbase");
    AddUnwindModule(nullptr, "exe");
    HANDLE sampler = CreateThread(nullptr, 0, &LayoutSamplerThread, nullptr, 0, nullptr);
    if (sampler) {
        SetThreadPriority(sampler, THREAD_PRIORITY_ABOVE_NORMAL);
        CloseHandle(sampler);
    }
    Log("[ui] layout sampler started, %d modules with unwind tables", g_unwindModuleCount);
}

// ---------------------------------------------------------------------------
// The game's UI frame
// ---------------------------------------------------------------------------

static void Hook_PostFrame(void* evoUi, float uiClock, void* arg3, void* arg4)
{
    NoteThread(g_postFrameThread, "the UI frame post");
    AcquireSRWLockExclusive(&g_statsLock);
    NoteClock(g_stats.postClock, g_lastPostClock, g_postClockSeen, uiClock);
    ReleaseSRWLockExclusive(&g_statsLock);
    g_origPostFrame(evoUi, uiClock, arg3, arg4);
}

static void Hook_EndFrame(void* evoUi, void* renderer, void* arg3, void* arg4)
{
    NoteThread(g_endFrameThread, "the UI frame end");
    int64_t started = Qpc();
    g_origEndFrame(evoUi, renderer, arg3, arg4);
    uint64_t us = ElapsedUs(started);
    g_endFrameUsSincePresent.fetch_add(us, std::memory_order_relaxed);

    AcquireSRWLockExclusive(&g_statsLock);
    g_stats.endFrames++;
    g_stats.endFrameUs += us;
    g_stats.endFrameMaxUs = std::max(g_stats.endFrameMaxUs, us);
    ReleaseSRWLockExclusive(&g_statsLock);
}

static void InstallUiFrameHooks()
{
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    auto nt = (IMAGE_NT_HEADERS64*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != kTimeDateStamp || nt->OptionalHeader.SizeOfImage != kSizeOfImage) {
        Log("[ui] this is not the game build the UI frame hooks were written for, only the Cohtml side of the probe runs");
        return;
    }
    void** vtable = (void**)(base + kRvaEvoUiVtable);
    if ((BYTE*)vtable[kPostFrameSlot] != base + kRvaPostFrameThunk || (BYTE*)vtable[kEndFrameSlot] != base + kRvaEndFrameThunk) {
        Log("[ui] the game UI's vtable is not where it was read, frame post and end are not timed");
        return;
    }
    HookVtableSlot(vtable, kPostFrameSlot, (void*)&Hook_PostFrame, (void**)&g_origPostFrame, "the game UI frame post");
    HookVtableSlot(vtable, kEndFrameSlot, (void*)&Hook_EndFrame, (void**)&g_origEndFrame, "the game UI frame end");
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

void InstallUiProbe()
{
    if (!g_cfg.uiProbe) return;
    QueryPerformanceFrequency(&g_qpf);
    InstallCohtmlHooks();
    InstallUiFrameHooks();
}

uint32_t UiProbeTakeEndFrameUs()
{
    return (uint32_t)std::min<uint64_t>(g_endFrameUsSincePresent.exchange(0, std::memory_order_relaxed), UINT32_MAX);
}

uint32_t UiProbeTakeAdvanceUs()
{
    return (uint32_t)std::min<uint64_t>(g_advanceUsSincePresent.exchange(0, std::memory_order_relaxed), UINT32_MAX);
}

static int AppendClock(char* line, int length, size_t size, const char* title, const ClockStats& clock)
{
    if (!clock.calls || length <= 0) return length;
    double realMs = (clock.lastQpc - clock.firstQpc) * 1000.0 / g_qpf.QuadPart;
    return length + _snprintf_s(line + length, size - length, _TRUNCATE,
        " | %s %u calls, from %.3f to %.3f in %.1f ms real, largest step %.3f, backwards %u",
        title, clock.calls, clock.first, clock.last, realMs, clock.largestStep, clock.backwards);
}

void UiProbeTick()
{
    if (!g_cfg.uiProbe) return;

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
    g_stats.postClock = {};
    g_stats.advanceClock = {};
    ReleaseSRWLockExclusive(&g_statsLock);

    char line[2048];
    int length = _snprintf_s(line, sizeof line, _TRUNCATE, "[ui] end frame %u, %.1f ms, max %.1f ms",
        second.endFrames, second.endFrameUs / 1000.0, second.endFrameMaxUs / 1000.0);
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
    length = AppendClock(line, length, sizeof line, "post clock", second.postClock);
    AppendClock(line, length, sizeof line, "view #1 clock", second.advanceClock);
    if (advanced || second.endFrames) Log("%s", line);
}
