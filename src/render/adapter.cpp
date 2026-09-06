#include "acevo/render/adapter.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/engine/flags.h"

// Measured on a 6 GB card: 1024 MB of tiles plus the engine's 1433 MB mesh cap leave about
// 600 MB of budget with everything else. Each step up keeps that margin on the next card size.
int AutoTilePoolMb(uint64_t vramMb)
{
    if (vramMb < 7168) return 1024;
    if (vramMb < 11264) return 1536;
    if (vramMb < 15360) return 2048;
    return 3072;
}

// The runtime keeps two staging buffers in video memory and the game's largest request is
// 32 MB, so 128 MB already holds four of them in flight.
int AutoStagingMb(uint64_t vramMb)
{
    if (vramMb < 7168) return 128;
    if (vramMb < 11264) return 192;
    return 256;
}

// The adapter with the most dedicated memory is the one the game renders on.
static bool DiscreteAdapter(IDXGIFactory1* factory, DXGI_ADAPTER_DESC1* out)
{
    bool found = false;
    SIZE_T best = 0;
    for (UINT i = 0;; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(i, &adapter) != S_OK || !adapter) break;
        DXGI_ADAPTER_DESC1 d = {};
        adapter->GetDesc1(&d);
        adapter->Release();
        if ((d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) || d.DedicatedVideoMemory <= best) continue;
        best = d.DedicatedVideoMemory;
        *out = d;
        found = true;
    }
    return found;
}

void ResolveAutoSizes(IDXGIFactory1* factory)
{
    static bool done = false;
    if (done || !factory) return;
    done = true;
    bool wantsAuto = g_cfg.stagingAuto;
    for (auto& f : g_cfg.flags) if (f.find(L"=auto") != std::wstring::npos) wantsAuto = true;
    if (!wantsAuto) return;

    DXGI_ADAPTER_DESC1 d = {};
    if (!DiscreteAdapter(factory, &d)) {
        Log("auto sizes: no adapter with dedicated memory found, the game's own values stay");
        return;
    }
    uint64_t vramMb = d.DedicatedVideoMemory >> 20;
    int tilePool = AutoTilePoolMb(vramMb);
    int staging = AutoStagingMb(vramMb);
    Log("auto sizes: '%ls' has %llu MB dedicated -> tile pool %d MB, staging buffer %d MB", d.Description, (unsigned long long)vramMb, tilePool, staging);
    if (g_cfg.stagingAuto) g_cfg.stagingMb = staging;
    ApplyAutoFlags(tilePool);
}

void LogDisplayOwner(IDXGIFactory1* factory, IUnknown* device, HWND hwnd)
{
    if (!factory || !device || !hwnd) return;
    LUID renderLuid = {};
    bool haveLuid = false;
    ID3D12CommandQueue* queue = nullptr;
    if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D12CommandQueue), (void**)&queue)) && queue) {
        ID3D12Device* d3d = nullptr;
        if (SUCCEEDED(queue->GetDevice(__uuidof(ID3D12Device), (void**)&d3d)) && d3d) {
            renderLuid = d3d->GetAdapterLuid();
            haveLuid = true;
            d3d->Release();
        }
        queue->Release();
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    for (UINT i = 0;; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(i, &adapter) != S_OK || !adapter) break;
        DXGI_ADAPTER_DESC1 ad = {};
        adapter->GetDesc1(&ad);
        for (UINT j = 0;; ++j) {
            IDXGIOutput* output = nullptr;
            if (adapter->EnumOutputs(j, &output) != S_OK || !output) break;
            DXGI_OUTPUT_DESC od = {};
            output->GetDesc(&od);
            output->Release();
            if (od.Monitor != monitor) continue;
            adapter->Release();
            if (!haveLuid) {
                Log("[display] the window's monitor %ls belongs to '%ls'", od.DeviceName, ad.Description);
                return;
            }
            bool same = ad.AdapterLuid.LowPart == renderLuid.LowPart && ad.AdapterLuid.HighPart == renderLuid.HighPart;
            if (same) {
                Log("[display] the window's monitor %ls belongs to the render adapter '%ls'", od.DeviceName, ad.Description);
                return;
            }
            Log("[display] WARNING: the window's monitor %ls belongs to '%ls', not to the adapter the game renders on. Every frame is copied to that adapter before it is shown, about a millisecond per frame and more on slow frames. A display wired to the render adapter, or the laptop's discrete graphics mode, avoids the copy.",
                od.DeviceName, ad.Description);
            return;
        }
        adapter->Release();
    }
    Log("[display] the window's monitor was not found among the adapters' outputs");
}
