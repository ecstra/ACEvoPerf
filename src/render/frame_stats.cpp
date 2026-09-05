#include "acevo/render/frame_stats.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/core/iat.h"
#include "acevo/dstorage/stats.h"
#include "acevo/telemetry/sampler.h"

static LARGE_INTEGER g_qpf = {};
static LARGE_INTEGER g_qpcStart = {};
static int64_t g_lastPresentQpc = 0;
std::atomic<uint64_t> g_frames{0}, g_frameSumUs{0}, g_frameMaxUs{0}, g_hitch20{0}, g_hitchCfg{0};
std::atomic<int> g_hitchLogBudget{5};
CRITICAL_SECTION g_frameCs;
std::vector<FrameSample> g_frameBuf;
static uint64_t g_hitchSnap[5] = {};
static uint64_t g_frameReqSnap[5] = {};
static UINT g_lastSyncInterval = 0xFFFFFFFF;
static double g_lastPresentCallMs = 0.0;   // how long the previous Present call blocked

// Every wait of the render thread is timed and sorted by what it waits for: an event a D3D12
// fence signals (the GPU), the swap chain's frame latency object, or anything else (worker
// threads, streaming). The exe's wait imports are hooked, fences are caught at creation so
// their events are known.
typedef DWORD (WINAPI *PFN_WaitForSingleObjectEx)(HANDLE, DWORD, BOOL);
typedef DWORD (WINAPI *PFN_WaitForSingleObject)(HANDLE, DWORD);
typedef DWORD (WINAPI *PFN_WaitForMultipleObjectsEx)(DWORD, const HANDLE*, BOOL, DWORD, BOOL);
typedef DWORD (WINAPI *PFN_WaitForMultipleObjects)(DWORD, const HANDLE*, BOOL, DWORD);
static PFN_WaitForSingleObjectEx g_origWaitForSingleObjectEx = nullptr;
static PFN_WaitForSingleObject g_origWaitForSingleObject = nullptr;
static PFN_WaitForMultipleObjectsEx g_origWaitForMultipleObjectsEx = nullptr;
static PFN_WaitForMultipleObjects g_origWaitForMultipleObjects = nullptr;
static HANDLE g_latencyObject = nullptr;
static std::atomic<DWORD> g_presentThreadId{0};
static double g_frameWaitMs = 0.0, g_frameFenceWaitMs = 0.0;

static const int kMaxFenceEvents = 128;
static HANDLE g_fenceEvents[kMaxFenceEvents];
static std::atomic<int> g_fenceEventCount{0};

static bool IsFenceEvent(HANDLE h)
{
    int n = std::min(g_fenceEventCount.load(), kMaxFenceEvents);
    for (int i = 0; i < n; ++i) if (g_fenceEvents[i] == h) return true;
    return false;
}

static void NoteFenceEvent(HANDLE h)
{
    if (IsFenceEvent(h)) return;
    int i = g_fenceEventCount.fetch_add(1);
    if (i < kMaxFenceEvents) g_fenceEvents[i] = h;
}

// per handle totals of the render thread's waits, logged once a minute
struct WaitTally { HANDLE handle; uint64_t us; uint32_t count; };
static WaitTally g_waitTally[32];
static int g_waitTallyCount = 0;
static double g_lastTallyLog = 0.0;

static void AccountWait(HANDLE handle, int64_t ticks)
{
    double ms = (double)ticks * 1000.0 / (double)g_qpf.QuadPart;
    g_frameWaitMs += ms;
    if (IsFenceEvent(handle)) g_frameFenceWaitMs += ms;
    for (int i = 0; i < g_waitTallyCount; ++i)
        if (g_waitTally[i].handle == handle) { g_waitTally[i].us += (uint64_t)(ms * 1000.0); g_waitTally[i].count++; return; }
    if (g_waitTallyCount < 32) g_waitTally[g_waitTallyCount++] = { handle, (uint64_t)(ms * 1000.0), 1 };
}

static void LogWaitTally()
{
    double now = NowSec();
    if (now - g_lastTallyLog < 60.0) return;
    g_lastTallyLog = now;
    for (int i = 0; i < g_waitTallyCount; ++i) {
        const WaitTally& w = g_waitTally[i];
        if (w.us < 20000) continue;
        Log("[wait] render thread waited %.1f ms in %u calls on %p%s in the last minute", w.us / 1000.0, w.count, w.handle,
            IsFenceEvent(w.handle) ? " (D3D12 fence event)" : w.handle == g_latencyObject ? " (frame latency object)" : "");
    }
    g_waitTallyCount = 0;
}

static DWORD WINAPI Hook_WaitForSingleObjectEx(HANDLE handle, DWORD timeout, BOOL alertable)
{
    if (GetCurrentThreadId() != g_presentThreadId.load()) return g_origWaitForSingleObjectEx(handle, timeout, alertable);
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    DWORD r = g_origWaitForSingleObjectEx(handle, timeout, alertable);
    QueryPerformanceCounter(&b);
    AccountWait(handle, b.QuadPart - a.QuadPart);
    return r;
}

static DWORD WINAPI Hook_WaitForSingleObject(HANDLE handle, DWORD timeout)
{
    if (GetCurrentThreadId() != g_presentThreadId.load()) return g_origWaitForSingleObject(handle, timeout);
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    DWORD r = g_origWaitForSingleObject(handle, timeout);
    QueryPerformanceCounter(&b);
    AccountWait(handle, b.QuadPart - a.QuadPart);
    return r;
}

static DWORD WINAPI Hook_WaitForMultipleObjectsEx(DWORD count, const HANDLE* handles, BOOL all, DWORD timeout, BOOL alertable)
{
    if (GetCurrentThreadId() != g_presentThreadId.load()) return g_origWaitForMultipleObjectsEx(count, handles, all, timeout, alertable);
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    DWORD r = g_origWaitForMultipleObjectsEx(count, handles, all, timeout, alertable);
    QueryPerformanceCounter(&b);
    HANDLE which = (count && handles) ? handles[r < count ? r : 0] : nullptr;
    AccountWait(which, b.QuadPart - a.QuadPart);
    return r;
}

static DWORD WINAPI Hook_WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL all, DWORD timeout)
{
    if (GetCurrentThreadId() != g_presentThreadId.load()) return g_origWaitForMultipleObjects(count, handles, all, timeout);
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    DWORD r = g_origWaitForMultipleObjects(count, handles, all, timeout);
    QueryPerformanceCounter(&b);
    HANDLE which = (count && handles) ? handles[r < count ? r : 0] : nullptr;
    AccountWait(which, b.QuadPart - a.QuadPart);
    return r;
}

// D3D12 fences: catch the device at creation, then every fence, then the events it signals.
// Command queues too: tile mapping updates and command list submits are timed per frame,
// both run on the GPU's queue and can hold the frame.
typedef HRESULT (WINAPI *PFN_D3D12CreateDevice)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateFence)(ID3D12Device*, UINT64, D3D12_FENCE_FLAGS, REFIID, void**);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetEventOnCompletion)(ID3D12Fence*, UINT64, HANDLE);
typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateCommandQueue)(ID3D12Device*, const D3D12_COMMAND_QUEUE_DESC*, REFIID, void**);
typedef void (STDMETHODCALLTYPE *PFN_UpdateTileMappings)(ID3D12CommandQueue*, ID3D12Resource*, UINT, const D3D12_TILED_RESOURCE_COORDINATE*, const D3D12_TILE_REGION_SIZE*, ID3D12Heap*, UINT, const D3D12_TILE_RANGE_FLAGS*, const UINT*, const UINT*, D3D12_TILE_MAPPING_FLAGS);
typedef void (STDMETHODCALLTYPE *PFN_ExecuteCommandLists)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
static PFN_D3D12CreateDevice g_origD3D12CreateDevice = nullptr;
static PFN_CreateFence g_origCreateFence = nullptr;
static PFN_SetEventOnCompletion g_origSetEventOnCompletion = nullptr;
static PFN_CreateCommandQueue g_origCreateCommandQueue = nullptr;
static PFN_UpdateTileMappings g_origUpdateTileMappings = nullptr;
static PFN_ExecuteCommandLists g_origExecuteCommandLists = nullptr;
static std::atomic<uint64_t> g_frameTileMapUs{0}, g_frameExecuteUs{0};
static std::atomic<uint32_t> g_frameMappedTiles{0};

static void STDMETHODCALLTYPE Hook_UpdateTileMappings(ID3D12CommandQueue* self, ID3D12Resource* resource, UINT regionCount, const D3D12_TILED_RESOURCE_COORDINATE* coords,
    const D3D12_TILE_REGION_SIZE* sizes, ID3D12Heap* heap, UINT rangeCount, const D3D12_TILE_RANGE_FLAGS* rangeFlags, const UINT* rangeStarts, const UINT* rangeTileCounts, D3D12_TILE_MAPPING_FLAGS flags)
{
    uint32_t tiles = 0;
    for (UINT i = 0; i < regionCount; ++i) tiles += sizes ? sizes[i].NumTiles : 1;
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    g_origUpdateTileMappings(self, resource, regionCount, coords, sizes, heap, rangeCount, rangeFlags, rangeStarts, rangeTileCounts, flags);
    QueryPerformanceCounter(&b);
    g_frameMappedTiles += tiles;
    g_frameTileMapUs += (uint64_t)((b.QuadPart - a.QuadPart) * 1000000 / g_qpf.QuadPart);
}

static void STDMETHODCALLTYPE Hook_ExecuteCommandLists(ID3D12CommandQueue* self, UINT count, ID3D12CommandList* const* lists)
{
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    g_origExecuteCommandLists(self, count, lists);
    QueryPerformanceCounter(&b);
    g_frameExecuteUs += (uint64_t)((b.QuadPart - a.QuadPart) * 1000000 / g_qpf.QuadPart);
}

// every queue of the device is hooked, they may or may not share a vtable
static void HookQueueSlot(void** vt, int idx, void* hook, void** orig, const char* what)
{
    if (vt[idx] == hook) return;
    if (*orig) {
        DWORD old = 0;
        if (!VirtualProtect(&vt[idx], sizeof(void*), PAGE_READWRITE, &old)) return;
        vt[idx] = hook;
        VirtualProtect(&vt[idx], sizeof(void*), old, &old);
        return;
    }
    HookVtableSlot(vt, idx, hook, orig, what);
}

static HRESULT STDMETHODCALLTYPE Hook_CreateCommandQueue(ID3D12Device* self, const D3D12_COMMAND_QUEUE_DESC* desc, REFIID riid, void** ppv)
{
    HRESULT hr = g_origCreateCommandQueue(self, desc, riid, ppv);
    if (SUCCEEDED(hr) && ppv && *ppv) {
        ID3D12CommandQueue* queue = nullptr;
        if (SUCCEEDED(((IUnknown*)*ppv)->QueryInterface(__uuidof(ID3D12CommandQueue), (void**)&queue)) && queue) {
            HookQueueSlot(*(void***)queue, 8, (void*)&Hook_UpdateTileMappings, (void**)&g_origUpdateTileMappings, "ID3D12CommandQueue::UpdateTileMappings");
            HookQueueSlot(*(void***)queue, 10, (void*)&Hook_ExecuteCommandLists, (void**)&g_origExecuteCommandLists, "ID3D12CommandQueue::ExecuteCommandLists");
            Log("D3D12 command queue created: type %d priority %d flags 0x%X", desc ? (int)desc->Type : -1, desc ? (int)desc->Priority : 0, desc ? (unsigned)desc->Flags : 0);
            queue->Release();
        }
    }
    return hr;
}

// With a null event the call itself blocks until the GPU reaches the value, so it is timed
// like a wait on a fence event.
static HRESULT STDMETHODCALLTYPE Hook_SetEventOnCompletion(ID3D12Fence* self, UINT64 value, HANDLE event)
{
    if (event) { NoteFenceEvent(event); return g_origSetEventOnCompletion(self, value, event); }
    if (GetCurrentThreadId() != g_presentThreadId.load()) return g_origSetEventOnCompletion(self, value, event);
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    HRESULT hr = g_origSetEventOnCompletion(self, value, event);
    QueryPerformanceCounter(&b);
    NoteFenceEvent(self);
    AccountWait(self, b.QuadPart - a.QuadPart);
    return hr;
}

static HRESULT STDMETHODCALLTYPE Hook_CreateFence(ID3D12Device* self, UINT64 initial, D3D12_FENCE_FLAGS flags, REFIID riid, void** ppv)
{
    HRESULT hr = g_origCreateFence(self, initial, flags, riid, ppv);
    if (SUCCEEDED(hr) && ppv && *ppv) {
        ID3D12Fence* fence = nullptr;
        if (SUCCEEDED(((IUnknown*)*ppv)->QueryInterface(__uuidof(ID3D12Fence), (void**)&fence)) && fence) {
            HookVtableSlot(*(void***)fence, 9, (void*)&Hook_SetEventOnCompletion, (void**)&g_origSetEventOnCompletion, "ID3D12Fence::SetEventOnCompletion");
            fence->Release();
        }
    }
    return hr;
}

// Waits are hooked in every module, so the render thread's waits inside d3d12 and the driver
// count too. The driver only exists after the device is created, so the patch runs again then.
static int PatchWaitImports()
{
    struct { const char* name; void* hook; void** orig; } hooks[] = {
        { "WaitForSingleObjectEx", (void*)&Hook_WaitForSingleObjectEx, (void**)&g_origWaitForSingleObjectEx },
        { "WaitForSingleObject", (void*)&Hook_WaitForSingleObject, (void**)&g_origWaitForSingleObject },
        { "WaitForMultipleObjectsEx", (void*)&Hook_WaitForMultipleObjectsEx, (void**)&g_origWaitForMultipleObjectsEx },
        { "WaitForMultipleObjects", (void*)&Hook_WaitForMultipleObjects, (void**)&g_origWaitForMultipleObjects },
    };
    int patched = 0;
    for (auto& h : hooks) patched += PatchEverywhere(h.name, h.hook, h.orig);
    return patched;
}

static HRESULT WINAPI Hook_D3D12CreateDevice(IUnknown* adapter, D3D_FEATURE_LEVEL level, REFIID riid, void** ppv)
{
    HRESULT hr = g_origD3D12CreateDevice(adapter, level, riid, ppv);
    if (SUCCEEDED(hr) && ppv && *ppv) {
        ID3D12Device* device = nullptr;
        if (SUCCEEDED(((IUnknown*)*ppv)->QueryInterface(__uuidof(ID3D12Device), (void**)&device)) && device) {
            HookVtableSlot(*(void***)device, 36, (void*)&Hook_CreateFence, (void**)&g_origCreateFence, "ID3D12Device::CreateFence");
            HookVtableSlot(*(void***)device, 8, (void*)&Hook_CreateCommandQueue, (void**)&g_origCreateCommandQueue, "ID3D12Device::CreateCommandQueue");
            device->Release();
        }
        Log("wait hooks: %d more wait import slots patched after the D3D12 device was created", PatchWaitImports());
    }
    return hr;
}

void InstallWaitHooks()
{
    if (!g_cfg.frameStats) return;
    int patched = PatchWaitImports();
    HMODULE d3d12 = GetModuleHandleW(L"d3d12.dll");
    int d3dPatched = 0;
    if (d3d12) {
        g_origD3D12CreateDevice = (PFN_D3D12CreateDevice)GetProcAddress(d3d12, "D3D12CreateDevice");
        if (g_origD3D12CreateDevice) d3dPatched = PatchIatByAddress(GetModuleHandleW(nullptr), (void*)g_origD3D12CreateDevice, (void*)&Hook_D3D12CreateDevice);
    }
    Log("wait hooks: %d wait import slots patched, D3D12CreateDevice %s", patched, d3dPatched ? "hooked" : "not hooked");
}

static void HookLatencyWaits(IDXGISwapChain2* sc2)
{
    if (g_latencyObject) return;
    g_latencyObject = sc2->GetFrameLatencyWaitableObject();
    Log("frame latency waitable object %p", g_latencyObject);
}

// Optional frame limiter: hold the present thread until the frame interval has passed.
// Sleeps while more than two milliseconds remain (the mod runs a 0.5 ms timer), spins
// the rest, so the interval is met within a few tens of microseconds.
static void LimitFrameRate()
{
    if (g_cfg.fpsLimit <= 0 || !g_lastPresentQpc) return;
    int64_t target = g_lastPresentQpc + g_qpf.QuadPart / g_cfg.fpsLimit;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    while (now.QuadPart < target) {
        int64_t remainingUs = (target - now.QuadPart) * 1000000 / g_qpf.QuadPart;
        if (remainingUs > 2000) Sleep(1);
        else YieldProcessor();
        QueryPerformanceCounter(&now);
    }
}

void InitFrameStats()
{
    InitializeCriticalSection(&g_frameCs);
    QueryPerformanceFrequency(&g_qpf);
    QueryPerformanceCounter(&g_qpcStart);
}

double NowSec()
{
    LARGE_INTEGER n; QueryPerformanceCounter(&n);
    return (double)(n.QuadPart - g_qpcStart.QuadPart) / (double)g_qpf.QuadPart;
}

static void OnPresent(UINT syncInterval)
{
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    int64_t last = g_lastPresentQpc;
    g_lastPresentQpc = now.QuadPart;
    if (syncInterval != g_lastSyncInterval) {
        g_lastSyncInterval = syncInterval;
        Log("Present sync interval = %u (%s)", syncInterval, syncInterval ? "vsync on" : "vsync off");
    }
    if (!last) return;

    double ms = (double)(now.QuadPart - last) * 1000.0 / (double)g_qpf.QuadPart;
    if (ms > 2000.0) return;                     // alt-tab or loading screen pause, not a frame
    g_presentThreadId.store(GetCurrentThreadId());
    SamplerOnPresent(ms);
    uint64_t us = (uint64_t)(ms * 1000.0);
    g_frames++; g_frameSumUs += us;
    uint64_t prev = g_frameMaxUs.load();
    while (us > prev && !g_frameMaxUs.compare_exchange_weak(prev, us)) {}
    if (ms > 20.0) g_hitch20++;

    if (ms > (double)g_cfg.hitchMs) {
        g_hitchCfg++;
        if (g_hitchLogBudget.fetch_sub(1) > 0) {
            uint64_t cur[5]; for (int i = 0; i < 5; ++i) cur[i] = g_reqByDest[i].load();
            Log("[hitch] %.1f ms frame at t=%.2fs | since previous hitch: tiles %llu req, file->mem %llu req, mem->gpu %llu req",
                ms, NowSec(), (unsigned long long)(cur[4] - g_hitchSnap[4]), (unsigned long long)(cur[0] - g_hitchSnap[0]),
                (unsigned long long)(cur[1] + cur[2] - g_hitchSnap[1] - g_hitchSnap[2]));
            for (int i = 0; i < 5; ++i) g_hitchSnap[i] = cur[i];
        }
    }

    if (g_cfg.frames) {
        uint64_t req[5];
        for (int i = 0; i < 5; ++i) req[i] = g_reqByDest[i].load();
        FrameSample sample;
        sample.t = (float)NowSec();
        sample.ms = (float)ms;
        sample.present = (float)g_lastPresentCallMs;
        sample.wait = (float)g_frameWaitMs;
        sample.fence = (float)g_frameFenceWaitMs;
        g_frameWaitMs = 0.0; g_frameFenceWaitMs = 0.0;
        sample.tileMap = (float)(g_frameTileMapUs.exchange(0) / 1000.0);
        sample.execute = (float)(g_frameExecuteUs.exchange(0) / 1000.0);
        sample.mappedTiles = g_frameMappedTiles.exchange(0);
        LogWaitTally();
        sample.tiles = (uint32_t)(req[4] - g_frameReqSnap[4]);
        sample.f2m = (uint32_t)(req[0] - g_frameReqSnap[0]);
        sample.gpumem = (uint32_t)(req[1] + req[2] - g_frameReqSnap[1] - g_frameReqSnap[2]);
        for (int i = 0; i < 5; ++i) g_frameReqSnap[i] = req[i];
        EnterCriticalSection(&g_frameCs);
        if (g_frameBuf.size() < 200000) g_frameBuf.push_back(sample);
        LeaveCriticalSection(&g_frameCs);
    }
}

typedef HRESULT (STDMETHODCALLTYPE *PFN_Present)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT (STDMETHODCALLTYPE *PFN_Present1)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetMaximumFrameLatency)(IDXGISwapChain2*, UINT);
typedef HRESULT (STDMETHODCALLTYPE *PFN_ResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetFullscreenState)(IDXGISwapChain*, BOOL, IDXGIOutput*);
static PFN_Present g_origPresent = nullptr;
static PFN_Present1 g_origPresent1 = nullptr;
static PFN_SetMaximumFrameLatency g_origSetMaximumFrameLatency = nullptr;
static PFN_ResizeBuffers g_origResizeBuffers = nullptr;
static PFN_SetFullscreenState g_origSetFullscreenState = nullptr;

// The Present call is where a frame waits for the swap chain queue or the display, so its
// duration separates waiting from rendering in the frames CSV.
static HRESULT TimedPresent(IDXGISwapChain* self, UINT sync, UINT flags)
{
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    HRESULT hr = g_origPresent(self, sync, flags);
    QueryPerformanceCounter(&b);
    g_lastPresentCallMs = (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)g_qpf.QuadPart;
    return hr;
}

static HRESULT TimedPresent1(IDXGISwapChain1* self, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS* pp)
{
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    HRESULT hr = g_origPresent1(self, sync, flags, pp);
    QueryPerformanceCounter(&b);
    g_lastPresentCallMs = (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)g_qpf.QuadPart;
    return hr;
}

// The game manages the waitable swap chain itself. If it re applies its own latency after a
// focus change or a session restart, the pacing fix (DEC-006) silently goes away, so every
// call is logged and the configured value wins.
static HRESULT STDMETHODCALLTYPE Hook_SetMaximumFrameLatency(IDXGISwapChain2* self, UINT latency)
{
    UINT want = latency;
    if (g_cfg.maxFrameLatency > 0) want = (UINT)g_cfg.maxFrameLatency;
    HRESULT hr = g_origSetMaximumFrameLatency(self, want);
    Log("game called SetMaximumFrameLatency(%u) at t=%.2fs -> applied %u, hr=0x%08X", latency, NowSec(), want, (unsigned)hr);
    return hr;
}

static HRESULT STDMETHODCALLTYPE Hook_ResizeBuffers(IDXGISwapChain* self, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags)
{
    HRESULT hr = g_origResizeBuffers(self, count, width, height, format, flags);
    Log("swap chain ResizeBuffers %ux%u buffers=%u fmt=%u flags=0x%X at t=%.2fs -> hr=0x%08X", width, height, count, (unsigned)format, flags, NowSec(), (unsigned)hr);
    return hr;
}

static HRESULT STDMETHODCALLTYPE Hook_SetFullscreenState(IDXGISwapChain* self, BOOL fullscreen, IDXGIOutput* output)
{
    HRESULT hr = g_origSetFullscreenState(self, fullscreen, output);
    Log("swap chain SetFullscreenState(%d) at t=%.2fs -> hr=0x%08X", fullscreen, NowSec(), (unsigned)hr);
    return hr;
}

static HRESULT STDMETHODCALLTYPE Hook_Present(IDXGISwapChain* self, UINT sync, UINT flags)
{
    if (flags & DXGI_PRESENT_TEST) return g_origPresent(self, sync, flags);
    LimitFrameRate();
    OnPresent(sync);
    return TimedPresent(self, sync, flags);
}
static HRESULT STDMETHODCALLTYPE Hook_Present1(IDXGISwapChain1* self, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS* pp)
{
    if (flags & DXGI_PRESENT_TEST) return g_origPresent1(self, sync, flags, pp);
    LimitFrameRate();
    OnPresent(sync);
    return TimedPresent1(self, sync, flags, pp);
}

void HookSwapChain(IUnknown* sc)
{
    if (!sc || !g_cfg.frameStats) return;
    IDXGISwapChain1* sc1 = nullptr;
    if (FAILED(sc->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&sc1)) || !sc1) return;
    void** vt = *(void***)sc1;
    HookVtableSlot(vt, 8, (void*)&Hook_Present, (void**)&g_origPresent, "IDXGISwapChain::Present");
    HookVtableSlot(vt, 22, (void*)&Hook_Present1, (void**)&g_origPresent1, "IDXGISwapChain1::Present1");
    HookVtableSlot(vt, 10, (void*)&Hook_SetFullscreenState, (void**)&g_origSetFullscreenState, "IDXGISwapChain::SetFullscreenState");
    HookVtableSlot(vt, 13, (void*)&Hook_ResizeBuffers, (void**)&g_origResizeBuffers, "IDXGISwapChain::ResizeBuffers");
    IDXGISwapChain2* sc2 = nullptr;
    if (SUCCEEDED(sc1->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&sc2)) && sc2) {
        HookVtableSlot(*(void***)sc2, 31, (void*)&Hook_SetMaximumFrameLatency, (void**)&g_origSetMaximumFrameLatency, "IDXGISwapChain2::SetMaximumFrameLatency");
        HookLatencyWaits(sc2);
        sc2->Release();
    }
    DXGI_SWAP_CHAIN_DESC1 d = {};
    if (SUCCEEDED(sc1->GetDesc1(&d)))
        Log("swap chain: %ux%u fmt=%u buffers=%u swapEffect=%u flags=0x%X scaling=%u", d.Width, d.Height, (unsigned)d.Format, d.BufferCount, (unsigned)d.SwapEffect, d.Flags, (unsigned)d.Scaling);
    sc1->Release();
}
