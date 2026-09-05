#include "acevo/render/frame_stats.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/core/iat.h"
#include "acevo/dstorage/stats.h"

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
        sc2->Release();
    }
    DXGI_SWAP_CHAIN_DESC1 d = {};
    if (SUCCEEDED(sc1->GetDesc1(&d)))
        Log("swap chain: %ux%u fmt=%u buffers=%u swapEffect=%u flags=0x%X scaling=%u", d.Width, d.Height, (unsigned)d.Format, d.BufferCount, (unsigned)d.SwapEffect, d.Flags, (unsigned)d.Scaling);
    sc1->Release();
}
