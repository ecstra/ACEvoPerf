#include "acevo/ui/ui_probe.h"
#include "acevo/core/code_patch.h"
#include "acevo/core/config.h"
#include "acevo/core/iat.h"
#include "acevo/core/log.h"
#include "acevo/ui/child_removal_fix.h"
#include "acevo/ui/cohtml_hooks.h"
#include "acevo/ui/responsive_ui.h"
#include "acevo/ui/style_match_fix.h"

// Written against AssettoCorsaEVO.exe 0.9.1 (the Steam build of 2026-09-11) and
// cohtml.WindowsDesktop.dll 1.61.0.3, from the UI deep dive of 2026-09-14
// (.agent/docs/research/ui-lag-deepdive-2026-09-14.md).
//
// The Cohtml library, its views and the game's UI frame come from the shared hooks
// (acevo/ui/cohtml_hooks.h). Library slot 5 is ExecuteWork(type, mode, family), view slot 6
// Advance(double ms) returning the frame id, and slot 61 AddInitialScript(const char*), which runs at
// the start of every document the view loads afterwards. The game's UI frame end waits for the frame's
// UI job while running other queued jobs, which is how UI layout ends up holding the render thread.
//
// The layout sampler suspends a thread only while it is inside Cohtml's layout work, reads its stack
// with the unwind tables of the modules loaded at start, and resumes it before counting anything. It
// never allocates, locks or logs while a game thread is suspended.
//
// The style hooks patch Cohtml's own code, one function entry and four calls, only when the module's
// stamp, size and the patched bytes match what was read. See the style invalidation section.

static const uint64_t kLayoutWork = 1;

// The page script, tested outside the game against a stand in for the bundle before it went in. The
// page fixes it counts belong to the responsive UI, whose own script publishes their counts.
static const char kPageScript[] = R"js(
(function () {
    'use strict';
    if (window.__acevoUiProbe) return;
    window.__acevoUiProbe = true;

    var page = String(location.pathname || '').split('/').pop() || 'unknown';
    var onHud = page === 'hud.html';
    console.log('[ACEvoPerf] ui probe ' + page + ' loaded, responsive ui page fixes ' + (window.__acevoUiFixes ? 'on' : 'off'));

    var framesThisSecond = 0;
    var changes = { classOps: 0, styleWrites: 0, attrWrites: 0, inserts: 0, removes: 0, htmlSets: 0, textSets: 0 };
    var events = { mousemove: 0, hover: 0, transitions: 0, animations: 0 };
    var top = {};
    var topKeys = 0;

    // What happened in each frame, so a slow frame can be matched with the frames before it. The
    // layout a frame's changes cause runs after that frame's scripts and holds the next frame back.
    var kinds = ['hover', 'focus', 'class', 'classNoop', 'style', 'attr', 'dom', 'scroll'];
    var bit = {};
    kinds.forEach(function (kind, index) { bit[kind] = 1 << index; });
    var kindOfChange = { classOps: 'class', styleWrites: 'style', attrWrites: 'attr', inserts: 'dom', removes: 'dom', htmlSets: 'dom', textSets: 'dom' };
    var frameMask = 0;
    var previousMask = 0;
    var framesWith = kinds.map(function () { return 0; });
    var slowAfter = kinds.map(function () { return 0; });
    var slow = { frames: 0, alone: 0 };
    var lastFrameAt = 0;

    function describe(element) {
        if (!element || !element.tagName) return '?';
        var name = String(element.tagName).toLowerCase();
        if (element.id) return name + '#' + element.id;
        var list = element.classList;
        if (list && list.length) return name + '.' + list[0];
        return name;
    }

    function mark(kind) {
        frameMask |= bit[kind];
    }

    // Counts one change to the page, keyed by what changed and on which element, so the log names
    // the scripts that make the page lay itself out again.
    function note(kind, element, detail, frameKind) {
        changes[kind]++;
        mark(frameKind || kindOfChange[kind]);
        var key = kind + ' ' + describe(element) + (detail ? ' ' + detail : '');
        if (top[key] === undefined) {
            if (topKeys >= 2000) return;
            topKeys++;
            top[key] = 0;
        }
        top[key]++;
    }

    function slowReport() {
        if (!slow.frames) return '';
        var parts = kinds.map(function (kind, index) { return kind + ' ' + slowAfter[index] + '/' + framesWith[index]; });
        return ' | slow frames ' + slow.frames + ', after ' + parts.join(' ') + ', after nothing ' + slow.alone;
    }

    // The responsive UI's page fixes count what they did, this reads and resets those counts.
    function takeFixCounts() {
        var fixes = window.__acevoUiFixes;
        var taken = { navigation: { calls: 0, scans: 0, skipped: 0 }, setup: { inits: 0, ignored: 0 }, controls: { refreshes: 0, held: 0 } };
        if (!fixes) return taken;
        for (var group in taken) {
            for (var count in taken[group]) {
                taken[group][count] = fixes[group][count];
                fixes[group][count] = 0;
            }
        }
        return taken;
    }

    function report() {
        var fix = takeFixCounts();
        var total = changes.classOps + changes.styleWrites + changes.attrWrites + changes.inserts + changes.removes + changes.htmlSets + changes.textSets;
        if (total || fix.navigation.calls || fix.setup.inits || fix.controls.refreshes || slow.frames || events.hover) {
            var ranked = Object.keys(top).sort(function (a, b) { return top[b] - top[a]; }).slice(0, 10);
            console.log('[ACEvoPerf] ui changes ' + page + ' | frames ' + framesThisSecond +
                ' | class ' + changes.classOps + ' style ' + changes.styleWrites + ' attr ' + changes.attrWrites +
                ' insert ' + changes.inserts + ' remove ' + changes.removes + ' html ' + changes.htmlSets + ' text ' + changes.textSets +
                ' | mousemove ' + events.mousemove + ' hover ' + events.hover + ' transitions ' + events.transitions + ' animations ' + events.animations +
                ' | navigation calls ' + fix.navigation.calls + ' scans ' + fix.navigation.scans + ' skipped ' + fix.navigation.skipped +
                ' | setup init ' + fix.setup.inits + ' ignored ' + fix.setup.ignored +
                ' | controls refreshes ' + fix.controls.refreshes + ' held ' + fix.controls.held +
                slowReport() +
                ' | top ' + ranked.map(function (key) { return key + ' x' + top[key]; }).join('; '));
        }
        for (var a in changes) changes[a] = 0;
        for (var b in events) events[b] = 0;
        for (var k = 0; k < kinds.length; k++) framesWith[k] = slowAfter[k] = 0;
        slow.frames = slow.alone = 0;
        top = {};
        topKeys = 0;
        framesThisSecond = 0;
    }

    function closeFrame(now) {
        var closing = frameMask;
        frameMask = 0;
        for (var k = 0; k < kinds.length; k++) if (closing & (1 << k)) framesWith[k]++;
        if (lastFrameAt && now - lastFrameAt > 45) {
            slow.frames++;
            var before = closing | previousMask;
            if (!before) slow.alone++;
            for (var s = 0; s < kinds.length; s++) if (before & (1 << s)) slowAfter[s]++;
        }
        previousMask = closing;
        lastFrameAt = now;
    }

    // The HUD's bindings add and remove its widgets natively, where the change counters cannot see, and
    // a change in the HUD's top level restyles the whole HUD (BUG-029). Each one is logged with the model
    // values the top level's conditions read.
    var hudBody = null;
    var hudNodes = [];

    function describeNode(node) {
        return node.nodeType === 1 ? describe(node) : String(node.nodeName);
    }

    function hudModels() {
        var car = window.ModelCurrentCar || {};
        var ui = window.ModelUIState || {};
        var session = window.ModelUISessionState || {};
        var timing = window.ModelTiming || {};
        return 'wrong way ' + car.is_wrong_way + ' | online ' + ui.online_status + ' | game mode ' + ui.gamemode +
            ' | free cam ' + ui.is_free_cam + ' | session flags ' + session.end_session_flag +
            ' | wait time ' + JSON.stringify(session.wait_time) + ' | next session ' + JSON.stringify(session.time_to_next_session) +
            ' | timing ' + JSON.stringify(timing.current);
    }

    // Walks the top level in place and builds lists only when something changed, so a frame allocates nothing.
    function watchHudTopLevel() {
        if (!hudBody || !hudBody.isConnected) {
            hudBody = document.querySelector('ks-hud#mainHUD > .component-body');
            hudNodes = [];
            if (!hudBody) return;
        }
        var index = 0;
        var node = hudBody.firstChild;
        while (node && index < hudNodes.length && hudNodes[index] === node) {
            node = node.nextSibling;
            index++;
        }
        if (!node && index === hudNodes.length) return;

        var nodes = [];
        for (node = hudBody.firstChild; node; node = node.nextSibling) nodes.push(node);
        if (hudNodes.length) {
            var added = nodes.filter(function (each) { return hudNodes.indexOf(each) < 0; }).map(describeNode);
            var removed = hudNodes.filter(function (each) { return nodes.indexOf(each) < 0; }).map(describeNode);
            console.log('[ACEvoPerf] ui hud top level | added ' + (added.join(', ') || 'nothing') + ' | removed ' + (removed.join(', ') || 'nothing') +
                ' | nodes ' + hudNodes.length + ' to ' + nodes.length + ' | ' + hudModels());
        }
        hudNodes = nodes;
    }

    var lastReport = Date.now();
    (function countFrames() {
        framesThisSecond++;
        var now = Date.now();
        closeFrame(now);
        if (onHud) {
            try { watchHudTopLevel(); } catch (e) {}
        }
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

    // True when a class list call leaves the list as it was, checked before the call runs.
    function leavesClassesAlone(list, name, args) {
        if (!list || typeof list.contains !== 'function' || !args.length) return false;
        var i;
        if (name === 'add') {
            for (i = 0; i < args.length; i++) if (!list.contains(args[i])) return false;
            return true;
        }
        if (name === 'remove') {
            for (i = 0; i < args.length; i++) if (list.contains(args[i])) return false;
            return true;
        }
        if (name === 'toggle') return args.length > 1 && !!args[1] === list.contains(args[0]);
        if (name === 'replace') return !list.contains(args[0]);
        return false;
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
                var alone = leavesClassesAlone(self, name, args);
                note('classOps', self.__acevoOwner, name + ' ' + Array.prototype.slice.call(args, 0, 2).join(' ') + (alone ? ' unchanged' : ''), alone ? 'classNoop' : 'class');
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
        document.addEventListener('mouseover', function () { events.hover++; mark('hover'); }, true);
        document.addEventListener('focusin', function () { mark('focus'); }, true);
        document.addEventListener('focusout', function () { mark('focus'); }, true);
        document.addEventListener('scroll', function () { mark('scroll'); }, true);
        document.addEventListener('wheel', function () { mark('scroll'); }, true);
        document.addEventListener('transitionstart', function () { events.transitions++; }, true);
        document.addEventListener('animationstart', function () { events.animations++; }, true);
        console.log('[ACEvoPerf] ui probe change counters on ' + page + ': ' + wrapped.join(', '));
    }

    // On the HUD the counters wrap about 20,000 of the page's own writes a second, measured on 2026-09-15,
    // and still miss the bindings' changes, so the top level watcher above stands in for them there.
    if (!onHud) {
        try {
            installChangeCounters();
        } catch (e) {
            console.error('[ACEvoPerf] ui probe change counters failed', e);
        }
    }
})();
)js";

typedef uint64_t (*PFN_ExecuteWork)(void* library, uint64_t type, uint64_t mode, uint64_t family);
typedef uint64_t (*PFN_Advance)(void* view, double milliseconds, uint64_t arg3, uint64_t arg4);
typedef void (*PFN_AddInitialScript)(void* view, const char* script);

static PFN_ExecuteWork g_origExecuteWork = nullptr;
static PFN_Advance g_origAdvance = nullptr;

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
static thread_local int64_t t_endFrameStarted = 0;

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

// Code the mod placed into the game, with its own function table.
static void AddUnwindRange(const BYTE* base, size_t size, const RUNTIME_FUNCTION* functions, DWORD count, const char* label)
{
    if (g_unwindModuleCount >= kMaxUnwindModules) return;
    UnwindModule& entry = g_unwindModules[g_unwindModuleCount++];
    entry.base = (uintptr_t)base;
    entry.end = entry.base + size;
    entry.functions = functions;
    entry.count = count;
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
// Style invalidation
// ---------------------------------------------------------------------------

// Read from cohtml.WindowsDesktop.dll 1.61.0.3. A document keeps the nodes whose style changed in a
// set at +0x1D28, and the style update restyles the whole subtree of every node in it, because the
// collection walks all children of each one whatever changed. So a change costs the size of the
// subtree under the node it lands on, and these hooks name those nodes and count what each invalidation marks.
//   AddChangedNode(document, NodeRef*) at 0x3526C0 puts a node in the set.
//   RestyleChanged at 0x3FCC40 restyles the set, called from 0x35C7F8 and again from 0x35CDA1.
//   CollectRestyle(styler, set, restyle list, other list) at 0x3FA980, called from 0x3FCD90.
//   RestyleAll at 0x3FE6E0 restyles the document after a stylesheet change, called from 0x35C697.
// Both restyles take thirteen pointer arguments. The node fields are the ones the selector matcher
// reads, the element type byte at +0x28, the id atom at +0x1E8 and the class atoms at +0x1F0.

static const DWORD kCohtmlTimeDateStamp = 0x675439C7;
static const DWORD kCohtmlSizeOfImage = 0x0070F000;

static const uint32_t kRvaAddChangedNode = 0x3526C0;
static const uint64_t kAddChangedNodeEntryFnv = 0x778E22D8F9870F20ull;   // its first 16 bytes
static const uint32_t kRvaRestyleChanged = 0x3FCC40;
static const uint32_t kRvaCollectRestyle = 0x3FA980;
static const uint32_t kRvaRestyleAll = 0x3FE6E0;

struct CallSite {
    uint32_t rva;
    uint32_t target;
    const char* what;
};

static const CallSite kRestyleChangedCall = { 0x35C7F8, kRvaRestyleChanged, "the restyle of changed nodes" };
static const CallSite kRestyleChangedAgainCall = { 0x35CDA1, kRvaRestyleChanged, "the second restyle of changed nodes" };
static const CallSite kCollectRestyleCall = { 0x3FCD90, kRvaCollectRestyle, "the collection of nodes to restyle" };
static const CallSite kRestyleAllCall = { 0x35C697, kRvaRestyleAll, "the restyle of the whole document" };

// Invalidate(element, kind, first name, second name) at 0x37B690 marks the nodes a changed feature of
// the element can restyle, called from 0x37BD11 only. Kind 0 is a child list change (0x37B600 passes it
// with empty names), 2 an id (0x37DBE5 passes the id at +0x1E8), 3 a class, 5 a state such as hover, 7 an
// attribute. Every kind 0 reads the same invalidation set (0x3F24B0), and on the HUD it matched every node
// under the element, 878 marks for the HUD's top level on 2026-09-15.
static const CallSite kInvalidateCall = { 0x37BD11, 0x37B690, "the invalidation of a changed feature" };

namespace changed_node {
    static const ptrdiff_t kKind = 0x20;        // bit 0 set for an element, bit 1 set AddChangedNode leaves the node out
    static const ptrdiff_t kTree = 0x24;        // bit 0 set while the node is in the document
    static const ptrdiff_t kType = 0x28;
    static const ptrdiff_t kId = 0x1E8;
    static const ptrdiff_t kClasses = 0x1F0;
    static const ptrdiff_t kClassCount = 0x1F8;
}

typedef uint32_t (*PFN_Restyle)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef uint64_t (*PFN_CollectRestyle)(void* styler, void* changed, void* restyle, void* other);
typedef uint64_t (*PFN_Invalidate)(void* element, uint64_t kind, void* first, void* second);

static PFN_Restyle g_restyleChanged = nullptr;
static PFN_Restyle g_restyleAll = nullptr;
static PFN_CollectRestyle g_collectRestyle = nullptr;
static PFN_Invalidate g_invalidate = nullptr;

static const uint64_t kSlowRestyleUs = 15000;
static const int kSlowRestylesPerSecond = 8;

struct SlowRestyle {
    uint32_t us;
    uint32_t nodes;
    uint32_t changed;
    char roots[200];
};

struct RestyleSecond {
    uint32_t changedPasses, fullPasses;
    uint64_t changedUs, changedMaxUs, fullUs, fullMaxUs;
    uint64_t nodes;
    int slowCount;
    SlowRestyle slow[kSlowRestylesPerSecond];
};

// What the collection of the restyle running on this thread found, read when the restyle returns.
struct Collection {
    uint32_t nodes;
    uint32_t changed;
    char roots[200];
};

static const int kInvalidationKinds = 8;
static const int kInvalidatedElements = 256;

struct InvalidationKind {
    uint32_t calls;
    uint64_t marks;
    uint32_t maxMarks;
};

struct InvalidatedElement {
    char element[80];
    uint8_t kind;
    uint32_t calls;
    uint64_t marks;
};

// A child list change that marks this many nodes restyles most of a page. The first one of each second
// keeps the call stack that led to it, to tell a binding's change from a script's.
static const uint8_t kChildListKind = 0;
static const uint32_t kBigChildListMarks = 200;
static const int kBigChildListFrames = 12;

struct BigChildList {
    uint32_t count;
    char element[80];
    uint32_t marks;
    USHORT frames;
    void* callers[kBigChildListFrames];
};

static RestyleSecond g_restyles = {};       // under g_statsLock
static thread_local Collection t_collection;
static thread_local uint32_t t_marks = 0;
static InvalidationKind g_invalidationKinds[kInvalidationKinds];         // under g_markLock
static InvalidatedElement g_invalidatedElements[kInvalidatedElements];   // under g_markLock
static BigChildList g_bigChildList = {};                                 // under g_markLock
static SRWLOCK g_markLock = SRWLOCK_INIT;

// The entry of AddChangedNode starts with mov [rsp+0x20], rbp, five bytes, which the jump to this stub
// replaces. The stub keeps the argument registers, hands the hook the stack pointer and frame pointer
// the call arrived with, replays the moved instruction and jumps back past it.
static const BYTE kMarkStub[] = {
    0x51,                                       // push rcx
    0x52,                                       // push rdx
    0x41, 0x50,                                 // push r8
    0x41, 0x51,                                 // push r9
    0x48, 0x83, 0xEC, 0x28,                     // sub rsp, 0x28
    0x4C, 0x8D, 0x44, 0x24, 0x48,               // lea r8, [rsp+0x48]
    0x4C, 0x8B, 0xCD,                           // mov r9, rbp
    0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,         // mov rax, OnAddChangedNode
    0xFF, 0xD0,                                 // call rax
    0x48, 0x83, 0xC4, 0x28,                     // add rsp, 0x28
    0x41, 0x59,                                 // pop r9
    0x41, 0x58,                                 // pop r8
    0x5A,                                       // pop rdx
    0x59,                                       // pop rcx
    0x48, 0x89, 0x6C, 0x24, 0x20,               // mov [rsp+0x20], rbp
    0xFF, 0x25, 0, 0, 0, 0,                     // jmp [rip]
    0, 0, 0, 0, 0, 0, 0, 0,                     // AddChangedNode + 5
};
static const size_t kMarkStubHookAt = 20;
static const size_t kMarkStubBackAt = sizeof kMarkStub - 8;

static int AppendAtom(char* out, int length, int size, char lead, const char* atom)
{
    if (!atom || !atom[0] || length >= size - 2) return length;
    out[length++] = lead;
    for (int i = 0; atom[i] && i < 32 && length < size - 1; ++i) {
        char c = atom[i];
        out[length++] = (c >= 32 && c < 127) ? c : '?';
    }
    out[length] = 0;
    return length;
}

// The changed set also holds nodes that are not elements and elements already taken out of the page,
// whose id and class fields are not there or already freed. The game's crash logger stops the thread
// that faults for 120 to 210 ms before any handler runs, measured on 2026-09-15, so only a connected
// element has its names read, and the handler stays for a node that still breaks that.
static void DescribeNode(const BYTE* node, char* out, int size)
{
    __try {
        int length = _snprintf_s(out, size, _TRUNCATE, "t%02X", node[changed_node::kType]);
        if (length < 0) return;
        bool element = *(const uint32_t*)(node + changed_node::kKind) & 1;
        bool connected = *(const uint32_t*)(node + changed_node::kTree) & 1;
        if (!element || !connected) {
            _snprintf_s(out + length, size - length, _TRUNCATE, element ? " gone" : " node");
            return;
        }
        length = AppendAtom(out, length, size, '#', *(const char* const*)(node + changed_node::kId));
        const char* const* classes = *(const char* const* const*)(node + changed_node::kClasses);
        uint32_t classCount = *(const uint32_t*)(node + changed_node::kClassCount);
        for (uint32_t i = 0; classes && i < classCount && i < 3; ++i) length = AppendAtom(out, length, size, '.', classes[i]);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        strcpy_s(out, size, "unreadable");
    }
}

// Counts the nodes AddChangedNode takes, for the invalidation that is running on this thread. It reads
// the same two fields AddChangedNode reads right after, so the node is as safe to read as it is there.
// Called up to a million times a second while a page restyles itself on every hover, so it only counts.
static void OnAddChangedNode(void*, void** nodeRef, const uint64_t*, uint64_t)
{
    const BYTE* node = (const BYTE*)*nodeRef;
    if (!node) return;
    if (!(*(const uint32_t*)(node + changed_node::kTree) & 1)) return;
    if (*(const uint32_t*)(node + changed_node::kKind) & 2) return;
    t_marks++;
}

// The set is a table of node pointers with empty buckets null, its bucket count at +8 and its size
// at +0x10. The first few nodes are named.
static void ReadChangedSet(const BYTE* set, Collection& collection)
{
    __try {
        const BYTE* const* buckets = *(const BYTE* const* const*)set;
        uint32_t bucketCount = *(const uint32_t*)(set + 8);
        collection.changed = (uint32_t)*(const uint64_t*)(set + 0x10);
        int length = 0;
        int named = 0;
        for (uint32_t i = 0; buckets && i < bucketCount && named < 3; ++i) {
            if (!buckets[i]) continue;
            char name[80];
            DescribeNode(buckets[i], name, sizeof name);
            int written = _snprintf_s(collection.roots + length, sizeof collection.roots - length, _TRUNCATE, "%s%s", named ? ", " : "", name);
            if (written < 0) break;
            length += written;
            named++;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        strcpy_s(collection.roots, "unreadable");
    }
}

// The restyle list is a vector, its count at +8.
static uint32_t ReadListCount(const BYTE* list)
{
    __try {
        return *(const uint32_t*)(list + 8);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// Counts the nodes one invalidation marked, by kind and by the element it started from.
static uint64_t Hook_Invalidate(void* element, uint64_t kind, void* first, void* second)
{
    uint32_t marksBefore = t_marks;
    uint64_t result = g_invalidate(element, kind, first, second);
    uint32_t marks = t_marks - marksBefore;

    char description[80];
    DescribeNode((const BYTE*)element, description, sizeof description);
    uint8_t kindByte = (uint8_t)kind;
    size_t slot = (size_t)((Fnv1a64((const BYTE*)description, strlen(description)) ^ kindByte) & (kInvalidatedElements - 1));

    void* callers[kBigChildListFrames];
    USHORT frames = 0;
    bool bigChildList = kindByte == kChildListKind && marks >= kBigChildListMarks;
    if (bigChildList) frames = RtlCaptureStackBackTrace(1, kBigChildListFrames, callers, nullptr);

    AcquireSRWLockExclusive(&g_markLock);
    if (bigChildList && !g_bigChildList.count++) {
        strcpy_s(g_bigChildList.element, description);
        g_bigChildList.marks = marks;
        g_bigChildList.frames = frames;
        memcpy(g_bigChildList.callers, callers, frames * sizeof callers[0]);
    }
    InvalidationKind& stats = g_invalidationKinds[std::min<int>(kindByte, kInvalidationKinds - 1)];
    stats.calls++;
    stats.marks += marks;
    stats.maxMarks = std::max(stats.maxMarks, marks);
    for (int probe = 0; probe < kInvalidatedElements; ++probe, slot = (slot + 1) & (kInvalidatedElements - 1)) {
        InvalidatedElement& entry = g_invalidatedElements[slot];
        if (!entry.calls) {
            strcpy_s(entry.element, description);
            entry.kind = kindByte;
            entry.calls = 1;
            entry.marks = marks;
            break;
        }
        if (entry.kind == kindByte && strcmp(entry.element, description) == 0) {
            entry.calls++;
            entry.marks += marks;
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_markLock);
    return result;
}

static uint64_t Hook_CollectRestyle(void* styler, void* changed, void* restyle, void* other)
{
    ReadChangedSet((const BYTE*)changed, t_collection);
    uint64_t result = g_collectRestyle(styler, changed, restyle, other);
    t_collection.nodes = ReadListCount((const BYTE*)restyle);
    return result;
}

static uint32_t Hook_RestyleChanged(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6, void* arg7,
    void* arg8, void* arg9, void* arg10, void* arg11, void* arg12, void* arg13)
{
    t_collection = {};
    int64_t started = Qpc();
    uint32_t result = g_restyleChanged(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13);
    uint64_t us = ElapsedUs(started);

    AcquireSRWLockExclusive(&g_statsLock);
    g_restyles.changedPasses++;
    g_restyles.changedUs += us;
    g_restyles.changedMaxUs = std::max(g_restyles.changedMaxUs, us);
    g_restyles.nodes += t_collection.nodes;
    if (us >= kSlowRestyleUs && g_restyles.slowCount < kSlowRestylesPerSecond) {
        SlowRestyle& slow = g_restyles.slow[g_restyles.slowCount++];
        slow.us = (uint32_t)us;
        slow.nodes = t_collection.nodes;
        slow.changed = t_collection.changed;
        strcpy_s(slow.roots, t_collection.roots);
    }
    ReleaseSRWLockExclusive(&g_statsLock);
    return result;
}

static uint32_t Hook_RestyleAll(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5, void* arg6, void* arg7,
    void* arg8, void* arg9, void* arg10, void* arg11, void* arg12, void* arg13)
{
    int64_t started = Qpc();
    uint32_t result = g_restyleAll(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13);
    uint64_t us = ElapsedUs(started);

    AcquireSRWLockExclusive(&g_statsLock);
    g_restyles.fullPasses++;
    g_restyles.fullUs += us;
    g_restyles.fullMaxUs = std::max(g_restyles.fullMaxUs, us);
    ReleaseSRWLockExclusive(&g_statsLock);
    return result;
}

static bool CallsTarget(const BYTE* base, const CallSite& site)
{
    if (base[site.rva] != 0xE8) return false;
    int32_t rel;
    memcpy(&rel, base + site.rva + 1, 4);
    return (int64_t)site.rva + 5 + rel == (int64_t)site.target;
}

static void InstallStyleHooks()
{
    BYTE* base = (BYTE*)GetModuleHandleW(L"cohtml.WindowsDesktop.dll");
    if (!base) return;
    auto nt = (IMAGE_NT_HEADERS64*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != kCohtmlTimeDateStamp || nt->OptionalHeader.SizeOfImage != kCohtmlSizeOfImage) {
        Log("[ui] this is not the Cohtml build the style hooks were written for (stamp %08X, image %08X), restyles are not traced",
            (unsigned)nt->FileHeader.TimeDateStamp, (unsigned)nt->OptionalHeader.SizeOfImage);
        return;
    }
    if (Fnv1a64(base + kRvaAddChangedNode, 16) != kAddChangedNodeEntryFnv) {
        Log("[ui] AddChangedNode at rva 0x%06X is not the code it was read as, restyles are not traced", kRvaAddChangedNode);
        return;
    }
    const CallSite* sites[] = { &kRestyleChangedCall, &kRestyleChangedAgainCall, &kCollectRestyleCall, &kRestyleAllCall, &kInvalidateCall };
    for (const CallSite* site : sites) {
        if (!CallsTarget(base, *site)) {
            Log("[ui] %s at rva 0x%06X is not the call it was read as, restyles are not traced", site->what, site->rva);
            return;
        }
    }

    const size_t page = 0x1000;
    BYTE* cave = AllocNear(base + kRvaAddChangedNode, page);
    if (!cave) {
        Log("[ui] no free memory within reach of Cohtml, restyles are not traced");
        return;
    }

    BYTE* markStub = cave;
    memcpy(markStub, kMarkStub, sizeof kMarkStub);
    void* markHook = (void*)&OnAddChangedNode;
    memcpy(markStub + kMarkStubHookAt, &markHook, 8);
    BYTE* markBack = base + kRvaAddChangedNode + 5;
    memcpy(markStub + kMarkStubBackAt, &markBack, 8);

    // jmp qword ptr [rip], followed by the absolute target
    BYTE* jumps = cave + sizeof kMarkStub;
    auto emitJump = [&jumps](void* target) {
        BYTE* start = jumps;
        const BYTE opcode[6] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
        memcpy(jumps, opcode, sizeof opcode);
        memcpy(jumps + sizeof opcode, &target, 8);
        jumps += sizeof opcode + 8;
        return start;
    };
    BYTE* restyleChangedStub = emitJump((void*)&Hook_RestyleChanged);
    BYTE* collectStub = emitJump((void*)&Hook_CollectRestyle);
    BYTE* restyleAllStub = emitJump((void*)&Hook_RestyleAll);
    BYTE* invalidateStub = emitJump((void*)&Hook_Invalidate);

    DWORD old = 0;
    if (!VirtualProtect(cave, page, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(cave, 0, MEM_RELEASE);
        Log("[ui] could not make the style stubs executable, restyles are not traced");
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), cave, page);

    g_restyleChanged = (PFN_Restyle)(base + kRvaRestyleChanged);
    g_restyleAll = (PFN_Restyle)(base + kRvaRestyleAll);
    g_collectRestyle = (PFN_CollectRestyle)(base + kRvaCollectRestyle);
    g_invalidate = (PFN_Invalidate)(base + kInvalidateCall.target);

    struct Patch {
        BYTE* at;
        BYTE opcode;
        const BYTE* destination;
        int32_t rel;
    };
    Patch patches[] = {
        { base + kRvaAddChangedNode, 0xE9, markStub, 0 },
        { base + kRestyleChangedCall.rva, 0xE8, restyleChangedStub, 0 },
        { base + kRestyleChangedAgainCall.rva, 0xE8, restyleChangedStub, 0 },
        { base + kCollectRestyleCall.rva, 0xE8, collectStub, 0 },
        { base + kRestyleAllCall.rva, 0xE8, restyleAllStub, 0 },
        { base + kInvalidateCall.rva, 0xE8, invalidateStub, 0 },
    };
    // Every displacement is computed before any write, so a stub out of reach patches nothing.
    for (Patch& patch : patches) {
        int64_t rel = patch.destination - (patch.at + 5);
        if (rel > INT32_MAX || rel < INT32_MIN) {
            VirtualFree(cave, 0, MEM_RELEASE);
            Log("[ui] a style stub is out of reach of Cohtml, restyles are not traced");
            return;
        }
        patch.rel = (int32_t)rel;
    }
    int written = 0;
    for (Patch& patch : patches) {
        if (!VirtualProtect(patch.at, 5, PAGE_EXECUTE_READWRITE, &old)) {
            Log("[ui] could not make Cohtml's code at rva 0x%06X writable, that hook is off", (unsigned)(patch.at - base));
            continue;
        }
        patch.at[0] = patch.opcode;
        memcpy(patch.at + 1, &patch.rel, 4);
        DWORD ignored = 0;
        VirtualProtect(patch.at, 5, old, &ignored);
        FlushInstructionCache(GetCurrentProcess(), patch.at, 5);
        written++;
    }
    Log("[ui] style hooks on at %d of %d places, restyle passes are timed and each invalidation's marks are counted",
        written, (int)(sizeof patches / sizeof patches[0]));
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

static void OnView(void* view, int number, unsigned width, unsigned height, bool mainView)
{
    AcquireSRWLockExclusive(&g_statsLock);
    // Car displays are created again at every session load, so a full table gives up its oldest display.
    int slot = -1;
    for (int i = 0; i < g_stats.viewCount; ++i) if (g_stats.views[i].view == view) slot = i;
    if (slot < 0 && g_stats.viewCount < kMaxViews) slot = g_stats.viewCount++;
    if (slot < 0) {
        for (int i = 0; i < g_stats.viewCount; ++i)
            if (g_stats.views[i].view != g_mainView && (slot < 0 || g_stats.views[i].number < g_stats.views[slot].number)) slot = i;
    }
    if (slot >= 0) {
        g_stats.views[slot] = {};
        g_stats.views[slot].view = view;
        g_stats.views[slot].number = number;
        g_stats.views[slot].width = width;
        g_stats.views[slot].height = height;
    }
    if (mainView) g_mainView = view;
    ReleaseSRWLockExclusive(&g_statsLock);

    void** vtable = *(void***)view;
    HookVtableSlot(vtable, cohtml_slot::kViewAdvance, (void*)&Hook_Advance, (void**)&g_origAdvance, "Cohtml View::Advance");

    // The menu and HUD view gets the page script, the car displays keep their own.
    if (mainView) {
        auto addInitialScript = (PFN_AddInitialScript)vtable[cohtml_slot::kViewAddInitialScript];
        addInitialScript(view, kPageScript);
        Log("[ui] page script added to view #%d, it counts page changes and what the page fixes do", number);
    }
}

static void OnLibrary(void* library)
{
    HookVtableSlot(*(void***)library, cohtml_slot::kLibraryExecuteWork, (void*)&Hook_ExecuteWork, (void**)&g_origExecuteWork, "Cohtml Library::ExecuteWork");
}

// ---------------------------------------------------------------------------
// The game's UI frame
// ---------------------------------------------------------------------------

static void OnFramePost(float uiClock)
{
    NoteThread(g_postFrameThread, "the UI frame post");
    AcquireSRWLockExclusive(&g_statsLock);
    NoteClock(g_stats.postClock, g_lastPostClock, g_postClockSeen, uiClock);
    ReleaseSRWLockExclusive(&g_statsLock);
}

static void OnFrameEnd(bool after)
{
    if (!after) {
        NoteThread(g_endFrameThread, "the UI frame end");
        t_endFrameStarted = Qpc();
        return;
    }
    uint64_t us = ElapsedUs(t_endFrameStarted);
    g_endFrameUsSincePresent.fetch_add(us, std::memory_order_relaxed);

    AcquireSRWLockExclusive(&g_statsLock);
    g_stats.endFrames++;
    g_stats.endFrameUs += us;
    g_stats.endFrameMaxUs = std::max(g_stats.endFrameMaxUs, us);
    ReleaseSRWLockExclusive(&g_statsLock);
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

void InstallUiProbe()
{
    if (!g_cfg.uiProbe) return;
    QueryPerformanceFrequency(&g_qpf);
    AddCohtmlLibraryListener(&OnLibrary);
    AddCohtmlViewListener(&OnView);
    AddUiFramePostListener(&OnFramePost);
    AddUiFrameEndListener(&OnFrameEnd);

    AddUnwindModule(L"cohtml.WindowsDesktop.dll", "cohtml");
    AddUnwindModule(L"RenoirCore.WindowsDesktop.dll", "renoir");
    AddUnwindModule(L"v8.dll", "v8");
    AddUnwindModule(L"ucrtbase.dll", "ucrtbase");
    AddUnwindModule(L"ntdll.dll", "ntdll");
    AddUnwindModule(L"kernelbase.dll", "kernelbase");
    AddUnwindModule(nullptr, "exe");
    const BYTE* stubs = nullptr;
    size_t stubsSize = 0;
    const RUNTIME_FUNCTION* stubFunctions = nullptr;
    DWORD stubCount = 0;
    if (StyleMatchFixUnwind(&stubs, &stubsSize, &stubFunctions, &stubCount)) AddUnwindRange(stubs, stubsSize, stubFunctions, stubCount, "styles");
    InstallStyleHooks();
    HANDLE sampler = CreateThread(nullptr, 0, &LayoutSamplerThread, nullptr, 0, nullptr);
    if (sampler) {
        SetThreadPriority(sampler, THREAD_PRIORITY_ABOVE_NORMAL);
        CloseHandle(sampler);
    }
    Log("[ui] layout sampler started, %d modules with unwind tables", g_unwindModuleCount);
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

static void ReportRestyles(const RestyleSecond& restyles)
{
    if (!restyles.changedPasses && !restyles.fullPasses) return;
    Log("[ui] restyles %u, %.1f ms, max %.1f ms, %llu nodes | whole document restyles %u, %.1f ms, max %.1f ms",
        restyles.changedPasses, restyles.changedUs / 1000.0, restyles.changedMaxUs / 1000.0, (unsigned long long)restyles.nodes,
        restyles.fullPasses, restyles.fullUs / 1000.0, restyles.fullMaxUs / 1000.0);
    for (int i = 0; i < restyles.slowCount; ++i) {
        const SlowRestyle& slow = restyles.slow[i];
        Log("[ui] slow restyle %.1f ms, %u nodes from %u changed, %s", slow.us / 1000.0, slow.nodes, slow.changed, slow.roots);
    }
}

// Names an address by the module of the unwind tables it falls in, the same names the layout samples use.
static int AppendAddress(char* line, int length, size_t size, void* address)
{
    int moduleIndex = -1;
    FindFunction((uintptr_t)address, moduleIndex);
    if (moduleIndex < 0) return length + _snprintf_s(line + length, size - length, _TRUNCATE, " %p", address);
    const UnwindModule& module = g_unwindModules[moduleIndex];
    return length + _snprintf_s(line + length, size - length, _TRUNCATE, " %s+0x%llX", module.label,
        (unsigned long long)((uintptr_t)address - module.base));
}

static void ReportBigChildList(const BigChildList& big)
{
    if (!big.count) return;
    char line[1024];
    int length = _snprintf_s(line, sizeof line, _TRUNCATE, "[ui] big child list change on %s, %u marks, %u that second, called from",
        big.element, big.marks, big.count);
    for (USHORT i = 0; i < big.frames && length > 0; ++i) length = AppendAddress(line, length, sizeof line, big.callers[i]);
    Log("%s", line);
}

static void ReportInvalidations()
{
    InvalidationKind kinds[kInvalidationKinds];
    const int wanted = 8;
    InvalidatedElement top[wanted] = {};
    int found = 0;
    AcquireSRWLockExclusive(&g_markLock);
    BigChildList big = g_bigChildList;
    g_bigChildList = {};
    memcpy(kinds, g_invalidationKinds, sizeof kinds);
    for (int slot = 0; slot < kInvalidatedElements; ++slot) {
        const InvalidatedElement& entry = g_invalidatedElements[slot];
        if (!entry.calls) continue;
        int at;
        if (found < wanted) {
            at = found++;
        } else if (entry.marks > top[wanted - 1].marks) {
            at = wanted - 1;
        } else {
            continue;
        }
        top[at] = entry;
        for (; at > 0 && top[at].marks > top[at - 1].marks; --at) std::swap(top[at], top[at - 1]);
    }
    memset(g_invalidationKinds, 0, sizeof g_invalidationKinds);
    memset(g_invalidatedElements, 0, sizeof g_invalidatedElements);
    ReleaseSRWLockExclusive(&g_markLock);
    ReportBigChildList(big);
    if (!found) return;

    char line[3072];
    int length = _snprintf_s(line, sizeof line, _TRUNCATE, "[ui] invalidations");
    for (int kind = 0; kind < kInvalidationKinds && length > 0; ++kind) {
        if (!kinds[kind].calls) continue;
        length += _snprintf_s(line + length, sizeof line - length, _TRUNCATE, " | kind %d %u calls, %llu marks, max %u",
            kind, kinds[kind].calls, (unsigned long long)kinds[kind].marks, kinds[kind].maxMarks);
    }
    if (length > 0) length += _snprintf_s(line + length, sizeof line - length, _TRUNCATE, " | most marks");
    for (int i = 0; i < found && length > 0; ++i) {
        length += _snprintf_s(line + length, sizeof line - length, _TRUNCATE, "%s %s kind %u %u calls %llu marks",
            i ? ";" : "", top[i].element, top[i].kind, top[i].calls, (unsigned long long)top[i].marks);
    }
    Log("%s", line);
}

void UiProbeTick()
{
    if (!g_cfg.uiProbe) return;

    SecondStats second;
    RestyleSecond restyles;
    AcquireSRWLockExclusive(&g_statsLock);
    second = g_stats;
    restyles = g_restyles;
    g_restyles = {};
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
    uint32_t moved = ResponsiveUiTakeMovedWork();
    if (moved && length > 0) length += _snprintf_s(line + length, sizeof line - length, _TRUNCATE, " | resource work moved off the render thread %u", moved);
    length = AppendClock(line, length, sizeof line, "post clock", second.postClock);
    AppendClock(line, length, sizeof line, "main view clock", second.advanceClock);
    if (advanced || second.endFrames) Log("%s", line);

    ReportRestyles(restyles);
    ReportInvalidations();

    ChildRemovalCounts removals = ChildRemovalFixTakeCounts();
    if (removals.narrowed || removals.full) {
        Log("[ui] child removals %u marked only the %u children that look at their position, %u took the engine's own restyle",
            removals.narrowed, removals.marked, removals.full);
    }
}
