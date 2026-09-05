#include "acevo/render/dxgi_hooks.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/core/iat.h"
#include "acevo/render/frame_stats.h"
#include "acevo/render/gpu_timing.h"

typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateSwapChainForHwnd)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateSwapChain)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
static PFN_CreateDXGIFactory1 g_realCDF1 = nullptr;
static PFN_CreateDXGIFactory2 g_realCDF2 = nullptr;
static PFN_CreateSwapChainForHwnd g_origCSCFH = nullptr;
static PFN_CreateSwapChain g_origCSC = nullptr;

static HRESULT STDMETHODCALLTYPE Hook_CreateSwapChainForHwnd(IDXGIFactory2* self, IUnknown* device, HWND hwnd,
    const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fs, IDXGIOutput* out, IDXGISwapChain1** pp)
{
    DXGI_SWAP_CHAIN_DESC1 d = *desc;
    bool flip = (d.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD || d.SwapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL);
    bool tweak = g_cfg.maxFrameLatency > 0 && flip;
    if (tweak) d.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    Log("CreateSwapChainForHwnd %ux%u fmt=%u buffers=%u swapEffect=%u flags=0x%X%s", d.Width, d.Height, (unsigned)d.Format, d.BufferCount, (unsigned)d.SwapEffect, d.Flags, tweak ? " (+waitable)" : "");
    HRESULT hr = g_origCSCFH(self, device, hwnd, &d, fs, out, pp);
    if (FAILED(hr) && tweak) {
        Log("  failed (0x%08X); retrying with original flags", (unsigned)hr);
        hr = g_origCSCFH(self, device, hwnd, desc, fs, out, pp);
        tweak = false;
    }
    if (FAILED(hr) || !pp || !*pp) return hr;

    if (tweak) {
        IDXGISwapChain2* sc2 = nullptr;
        if (SUCCEEDED((*pp)->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&sc2))) {
            HRESULT h2 = sc2->SetMaximumFrameLatency((UINT)g_cfg.maxFrameLatency);
            Log("  SetMaximumFrameLatency(%d) -> hr=0x%08X", g_cfg.maxFrameLatency, (unsigned)h2);
            sc2->Release();
        }
    }
    HookSwapChain(*pp);
    GpuTimingSetPresentQueue(device, (PFN_ExecuteCommandListsOriginal)OriginalExecuteCommandLists());
    return hr;
}
static HRESULT STDMETHODCALLTYPE Hook_CreateSwapChain(IDXGIFactory* self, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** pp)
{
    Log("CreateSwapChain (legacy) %ux%u buffers=%u swapEffect=%u flags=0x%X", desc->BufferDesc.Width, desc->BufferDesc.Height, desc->BufferCount, (unsigned)desc->SwapEffect, desc->Flags);
    HRESULT hr = g_origCSC(self, device, desc, pp);
    if (SUCCEEDED(hr) && pp && *pp) HookSwapChain(*pp);
    return hr;
}

static void HookFactoryVtable(void* factory)
{
    if (!factory) return;
    IDXGIFactory2* f2 = nullptr;
    if (FAILED(((IUnknown*)factory)->QueryInterface(__uuidof(IDXGIFactory2), (void**)&f2)) || !f2) return;
    void** vt = *(void***)f2;
    HookVtableSlot(vt, 15, (void*)&Hook_CreateSwapChainForHwnd, (void**)&g_origCSCFH, "IDXGIFactory2::CreateSwapChainForHwnd");
    HookVtableSlot(vt, 10, (void*)&Hook_CreateSwapChain, (void**)&g_origCSC, "IDXGIFactory::CreateSwapChain");
    f2->Release();
}
static HRESULT WINAPI Hook_CreateDXGIFactory1(REFIID riid, void** ppv)
{
    HRESULT hr = g_realCDF1(riid, ppv);
    if (SUCCEEDED(hr) && ppv) HookFactoryVtable(*ppv);
    return hr;
}
static HRESULT WINAPI Hook_CreateDXGIFactory2(UINT flags, REFIID riid, void** ppv)
{
    HRESULT hr = g_realCDF2(flags, riid, ppv);
    if (SUCCEEDED(hr) && ppv) HookFactoryVtable(*ppv);
    return hr;
}

void InstallDxgiHooks()
{
    if (!g_cfg.dxgiEnabled) return;
    HMODULE dxgi = GetModuleHandleW(L"dxgi.dll");
    if (!dxgi) { Log("DXGI: dxgi.dll not loaded at attach time; hook skipped"); return; }
    g_realCDF1 = (PFN_CreateDXGIFactory1)GetProcAddress(dxgi, "CreateDXGIFactory1");
    g_realCDF2 = (PFN_CreateDXGIFactory2)GetProcAddress(dxgi, "CreateDXGIFactory2");
    HMODULE exe = GetModuleHandleW(nullptr);
    int a = PatchIatByAddress(exe, (void*)g_realCDF1, (void*)&Hook_CreateDXGIFactory1);
    int b = PatchIatByAddress(exe, (void*)g_realCDF2, (void*)&Hook_CreateDXGIFactory2);
    Log("DXGI: IAT hooks in game exe: CreateDXGIFactory1=%d CreateDXGIFactory2=%d (frame_stats=%d max_frame_latency=%d)", a, b, g_cfg.frameStats, g_cfg.maxFrameLatency);
}
