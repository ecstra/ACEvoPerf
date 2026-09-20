#include "acevo/render/adapter.h"
#include "acevo/render/dxgi_hooks.h"   // PFN_CreateDXGIFactory1
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/engine/flags.h"

// Measured on a 6 GB card: 1024 MB of tiles plus the engine's 1433 MB mesh cap leave about
// 600 MB of budget with everything else. Each step up keeps that margin on the next card size.
// The two below it are the same budget worked backwards and were never measured on such a card,
// so they err small, and 256 is where the engine's own dynamic formula bottoms out.
//
// Writing nothing on a small card was tried and is worse than any number here. The shipped ini
// sets force_canonical_pool_sizes at the early pass, long before the card is known, and with
// that on and tile_pool_mb unwritten the engine takes the whole texturePoolSize define, 1433 MB
// at Low and 6144 at Ultra (DEC-005, engine-flags). So every card gets a figure.
int AutoTilePoolMb(uint64_t vramMb)
{
    if (vramMb < 3072) return 256;
    if (vramMb < 5120) return 512;
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

// The LUID of the adapter the sizes were picked from, kept so the real one can be checked
// against it once the game's device exists. The sizes cannot wait for that: on 2026-09-18 the
// engine sized its tile pool 7 ms before it created the swap chain, and the swap chain is the
// first place the render adapter's LUID can be read at all.
static LUID g_sizedFromLuid = {};
static bool g_sizedFromKnown = false;
static wchar_t g_sizedFromName[128] = {};
static uint64_t g_sizedFromMb = 0;

// The adapter with the most dedicated memory is the one the game renders on. The engine's own
// log says it enumerates adapters and names one it is "Using", and on the reference laptop it
// lists the discrete card only, so the two rules agree wherever a discrete card exists.
//
// An adapter reporting none at all is still taken, which is why the ranking is written the long
// way round. Plenty of integrated parts report zero and keep everything in shared memory, and
// passing on those would leave the canonical flag holding the whole define, the same failure as
// writing nothing on a small card (DEC-022).
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
        if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (found && d.DedicatedVideoMemory <= best) continue;
        best = d.DedicatedVideoMemory;
        *out = d;
        found = true;
    }
    return found;
}

static bool g_resolveDone = false;

static bool WantsAutoSizes()
{
    if (g_cfg.stagingAuto) return true;
    for (auto& f : g_cfg.flags) if (f.find(L"=auto") != std::wstring::npos) return true;
    return false;
}

void ResolveAutoSizes(IDXGIFactory1* factory)
{
    if (g_resolveDone || !factory) return;
    g_resolveDone = true;
    if (!WantsAutoSizes()) return;

    DXGI_ADAPTER_DESC1 d = {};
    if (!DiscreteAdapter(factory, &d)) {
        Log("auto sizes: the factory lists no adapter the game could render on, so nothing is sized. With force_canonical_pool_sizes on and tile_pool_mb left at auto the engine takes the whole texturePoolSize define, so set tile_pool_mb and staging_buffer_mb by hand if this run is not headless.");
        return;
    }
    uint64_t vramMb = d.DedicatedVideoMemory >> 20;
    int tilePool = AutoTilePoolMb(vramMb);
    int staging = AutoStagingMb(vramMb);
    g_sizedFromLuid = d.AdapterLuid;
    g_sizedFromMb = vramMb;
    wcsncpy_s(g_sizedFromName, d.Description, _TRUNCATE);
    g_sizedFromKnown = true;
    Log("auto sizes: '%ls' has %llu MB dedicated -> tile pool %d MB, staging buffer %d MB", d.Description, (unsigned long long)vramMb, tilePool, staging);
    if (g_cfg.stagingAuto) g_cfg.stagingMb = staging;
    ApplyAutoFlags(tilePool);
}

// The other chance to read the card, taken from the late flag pass inside DStorageGetFactory.
// That runs on the game's own thread rather than under the loader lock, and on 2026-09-18 it
// ran 361 ms before the engine sized its tile pool. It does nothing when the game's own factory
// has already been through here, which on 0.9.1 is always, since that arrives 1.9 seconds
// earlier. It covers the exe with no import to patch and any launch order that puts
// DirectStorage first, and creates a factory of its own the same way reflex and timeline do.
void ResolveAutoSizesFallback()
{
    if (g_resolveDone || !WantsAutoSizes()) return;
    // Loaded rather than looked up, because one of the ways to get here is dxgi.dll not being
    // loaded yet. This runs on the game's own thread, so a load is safe, and from System32 only,
    // since the game folder is searched first for a bare name and we ship files into it.
    HMODULE dxgi = LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    PFN_CreateDXGIFactory1 create = dxgi ? (PFN_CreateDXGIFactory1)GetProcAddress(dxgi, "CreateDXGIFactory1") : nullptr;
    if (!create) {
        Log("auto sizes: no DXGI factory has reached us and CreateDXGIFactory1 is not available, so nothing is sized and tile_pool_mb goes unwritten, which hands the size to force_canonical_pool_sizes if it is on and to the engine's own formula if it is not. Set tile_pool_mb and staging_buffer_mb by hand.");
        return;
    }
    IDXGIFactory1* own = nullptr;
    if (FAILED(create(__uuidof(IDXGIFactory1), (void**)&own)) || !own) {
        Log("auto sizes: no DXGI factory has reached us and our own could not be created, so nothing is sized and tile_pool_mb goes unwritten, which hands the size to force_canonical_pool_sizes if it is on and to the engine's own formula if it is not. Set tile_pool_mb and staging_buffer_mb by hand.");
        return;
    }
    Log("auto sizes: no DXGI factory has reached us by the first DirectStorage call, reading the card off our own instead");
    ResolveAutoSizes(own);
    own->Release();
}

// The swap chain's device parameter is the command queue on D3D12, which is the only path the
// game takes. Anything else leaves the LUID unknown and every caller stays quiet.
static bool RenderAdapterLuid(IUnknown* device, LUID* out)
{
    if (!device) return false;
    bool found = false;
    ID3D12CommandQueue* queue = nullptr;
    if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D12CommandQueue), (void**)&queue)) && queue) {
        ID3D12Device* d3d = nullptr;
        if (SUCCEEDED(queue->GetDevice(__uuidof(ID3D12Device), (void**)&d3d)) && d3d) {
            *out = d3d->GetAdapterLuid();
            found = true;
            d3d->Release();
        }
        queue->Release();
    }
    return found;
}

void CheckAutoSizeAdapter(IDXGIFactory1* factory, IUnknown* device)
{
    if (!g_sizedFromKnown || !factory) return;
    LUID renderLuid = {};
    if (!RenderAdapterLuid(device, &renderLuid)) return;
    g_sizedFromKnown = false;   // the answer cannot change, so say it once
    if (renderLuid.LowPart == g_sizedFromLuid.LowPart && renderLuid.HighPart == g_sizedFromLuid.HighPart) {
        Log("auto sizes: the game renders on '%ls', the card the sizes were picked from", g_sizedFromName);
        return;
    }

    for (UINT i = 0;; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(i, &adapter) != S_OK || !adapter) break;
        DXGI_ADAPTER_DESC1 ad = {};
        adapter->GetDesc1(&ad);
        adapter->Release();
        if (ad.AdapterLuid.LowPart != renderLuid.LowPart || ad.AdapterLuid.HighPart != renderLuid.HighPart) continue;
        Log("auto sizes: WARNING: the sizes were picked from '%ls' with %llu MB, but the game renders on '%ls' with %llu MB. The pool is already made by now, so set tile_pool_mb and staging_buffer_mb by hand in acevo_perf.ini for the card the game actually uses.",
            g_sizedFromName, (unsigned long long)g_sizedFromMb, ad.Description, (unsigned long long)(ad.DedicatedVideoMemory >> 20));
        return;
    }
    Log("auto sizes: WARNING: the sizes were picked from '%ls' with %llu MB, but the game renders on an adapter that is not in the factory's list at all.",
        g_sizedFromName, (unsigned long long)g_sizedFromMb);
}

void LogDisplayOwner(IDXGIFactory1* factory, IUnknown* device, HWND hwnd)
{
    if (!factory || !device || !hwnd) return;
    LUID renderLuid = {};
    bool haveLuid = RenderAdapterLuid(device, &renderLuid);

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
