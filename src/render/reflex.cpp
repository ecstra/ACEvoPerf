// NVIDIA Reflex, see include/acevo/render/reflex.h.
//
// nvapi64.dll exports exactly one symbol, `nvapi_QueryInterface`, and everything else is
// reached by asking it for a numeric id. The ids and the structure below are from NVIDIA's
// own headers (github.com/NVIDIA/nvapi, nvapi_interface.h and nvapi_lite_common.h), not
// guessed: a wrong layout here goes straight into the display driver.
#include "acevo/render/reflex.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/render/dxgi_hooks.h"   // PFN_CreateDXGIFactory1

namespace reflex {

// nvapi_interface.h
//
// NvAPI_Unload is not among these on purpose. The only place the mod could call it is the
// process detach in DllMain, and NVIDIA's own documentation says not to call it from DllMain.
// Windows tears the process down either way, so there is nothing to gain by doing it unsafely.
static const unsigned kId_Initialize      = 0x0150e828;
static const unsigned kId_D3D_SetSleepMode = 0xac1ca9e0;
static const unsigned kId_D3D_Sleep       = 0x852cd1d2;
static const unsigned kId_D3D_GetSleepStatus = 0xaef96ca1;

// nvapi_lite_common.h: NvU8 is unsigned char, NvBool is NvU8, NvU32 is 4 bytes, and
// MAKE_NVAPI_VERSION(t, v) is sizeof(t) | (v << 16). This is 44 bytes in both the version
// that ends in rsvd[31] and the one that added bUseMinQueueTime and shrank it to rsvd[30],
// so the constant is the same either way.
struct NV_SET_SLEEP_MODE_PARAMS {
    uint32_t version;
    uint8_t  bLowLatencyMode;
    uint8_t  bLowLatencyBoost;
    uint32_t minimumIntervalUs;
    uint8_t  bUseMarkersToOptimize;
    uint8_t  bUseMinQueueTime;
    uint8_t  rsvd[30];
};
static_assert(sizeof(NV_SET_SLEEP_MODE_PARAMS) == 44, "NVAPI sleep mode parameter block must stay 44 bytes");
static const uint32_t kSleepModeVersion = 44 | (1u << 16);

// The driver's own answer to "is low latency mode on", so the log reports what the driver
// thinks rather than only what we asked it for.
struct NV_GET_SLEEP_STATUS_PARAMS {
    uint32_t version;
    uint8_t  bLowLatencyMode;
    uint8_t  bFsVrr;
    uint8_t  bCplVsyncOn;
    uint8_t  rsvd[126];
};
static_assert(sizeof(NV_GET_SLEEP_STATUS_PARAMS) == 136, "NVAPI sleep status block must stay 136 bytes");
static const uint32_t kSleepStatusVersion = 136 | (1u << 16);

typedef void* (*PFN_QueryInterface)(unsigned id);
typedef int   (*PFN_Initialize)();
typedef int   (*PFN_SetSleepMode)(IUnknown* device, NV_SET_SLEEP_MODE_PARAMS* params);
typedef int   (*PFN_Sleep)(IUnknown* device);
typedef int   (*PFN_GetSleepStatus)(IUnknown* device, NV_GET_SLEEP_STATUS_PARAMS* params);

static PFN_SetSleepMode   g_setSleepMode = nullptr;
static PFN_Sleep          g_sleep = nullptr;
static PFN_GetSleepStatus g_getSleepStatus = nullptr;
// The D3D12 device the layer is bound to. It stands in for the old "we have tried once" flag,
// which a splash or overlay swap chain could spend before the game's own ever arrived, and which
// also meant a device reset was never noticed. A device that is already this one is nothing new,
// a different one is a rebind, and no device at all is a swap chain worth ignoring.
static std::atomic<IUnknown*> g_device{nullptr};
static IUnknown*        g_retiredDevice = nullptr;   // the previous one, freed at the next rebind
static IUnknown*        g_refusedDevice = nullptr;   // one we already said no to, compared only
// The swap chain the layer was set up from, held as an address to compare and nothing else. It
// is never dereferenced, called or released, so there is no object here to outlive. The one risk
// a bare address carries is that this swap chain dies and a later one is allocated at the same
// place, and that later one would be the game's replacement main swap chain, which is the one we
// would want to pace anyway. Keeping a reference instead would pin a dead swap chain alive,
// which is the fault F-07 was about.
//
// These are read once per present from the game's own thread and written from whichever thread
// creates a swap chain, which is not always the same one, so they are atomic.
static std::atomic<IUnknown*> g_swapChain{nullptr};
static bool             g_resolved = false;      // nvapi found and its entry points in hand
static bool             g_resolveFailed = false; // and it is not coming, which is a process wide answer
static bool             g_noDeviceLogged = false;
static std::atomic<bool> g_active{false};
static std::atomic<uint64_t> g_sleepCalls{0};
static std::atomic<uint64_t> g_sleepFailures{0};

static void ReportStatus(const char* when);

// Is the adapter the game renders on actually an NVIDIA one? Having nvapi64.dll on the
// machine does not mean it is: a laptop can carry NVIDIA's driver while the game renders on
// an AMD or Intel adapter, and a machine can have an idle NVIDIA card in it. Handing an
// adapter that is not NVIDIA's to NVAPI is not something to find out the hard way in someone
// else's game, so the vendor is checked first and nvapi is not even loaded otherwise.
static bool RenderAdapterIsNvidia(ID3D12Device* device)
{
    const UINT kNvidiaVendorId = 0x10DE;
    LUID want = device->GetAdapterLuid();

    HMODULE dxgi = GetModuleHandleW(L"dxgi.dll");
    auto createFactory = dxgi ? (PFN_CreateDXGIFactory1)GetProcAddress(dxgi, "CreateDXGIFactory1") : nullptr;
    if (!createFactory) { Log("[reflex] cannot reach dxgi to identify the render adapter, layer idle"); return false; }
    IDXGIFactory1* factory = nullptr;
    if (FAILED(createFactory(__uuidof(IDXGIFactory1), (void**)&factory)) || !factory) {
        Log("[reflex] a DXGI factory of our own could not be created to identify the render adapter, layer idle");
        return false;
    }

    bool nvidia = false;
    for (UINT i = 0;; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(i, &adapter) != S_OK || !adapter) break;
        DXGI_ADAPTER_DESC1 d = {};
        adapter->GetDesc1(&d);
        adapter->Release();
        if (d.AdapterLuid.LowPart == want.LowPart && d.AdapterLuid.HighPart == want.HighPart) {
            nvidia = (d.VendorId == kNvidiaVendorId);
            Log("[reflex] the game renders on '%ls' (vendor 0x%04X), %s", d.Description, d.VendorId,
                nvidia ? "NVIDIA, Reflex is available" : "not NVIDIA, Reflex stays off and nvapi is never loaded");
            factory->Release();
            return nvidia;
        }
    }
    factory->Release();
    Log("[reflex] the device's adapter is not in the factory's list, so its vendor is unknown, layer idle");
    return false;
}

static bool Resolve()
{
    // System32 only. A bare name searches the game folder first, and the mod ships files into
    // that folder, so anything dropped there beside them would be loaded in the driver's place.
    HMODULE nvapi = LoadLibraryExW(L"nvapi64.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!nvapi) {
        Log("[reflex] nvapi64.dll not present, no NVIDIA driver on this machine, layer idle");
        return false;
    }
    auto query = (PFN_QueryInterface)GetProcAddress(nvapi, "nvapi_QueryInterface");
    if (!query) { Log("[reflex] nvapi64.dll has no nvapi_QueryInterface, layer idle"); return false; }

    auto init = (PFN_Initialize)query(kId_Initialize);
    if (!init) { Log("[reflex] NvAPI_Initialize not offered by this driver, layer idle"); return false; }
    int status = init();
    if (status != 0) { Log("[reflex] NvAPI_Initialize failed (%d), layer idle", status); return false; }

    g_setSleepMode = (PFN_SetSleepMode)query(kId_D3D_SetSleepMode);
    g_sleep = (PFN_Sleep)query(kId_D3D_Sleep);
    g_getSleepStatus = (PFN_GetSleepStatus)query(kId_D3D_GetSleepStatus);
    if (!g_setSleepMode || !g_sleep) {
        Log("[reflex] this driver does not offer the Reflex entry points, layer idle");
        return false;
    }
    return true;
}

void OnSwapChain(IUnknown* swapChain)
{
    if (!g_cfg.reflex || !swapChain) return;

    // Reflex wants the D3D12 device, which the swap chain can hand over.
    IDXGISwapChain* sc = nullptr;
    if (FAILED(swapChain->QueryInterface(__uuidof(IDXGISwapChain), (void**)&sc)) || !sc) return;
    ID3D12Device* device = nullptr;
    HRESULT hr = sc->GetDevice(__uuidof(ID3D12Device), (void**)&device);
    sc->Release();
    if (FAILED(hr) || !device) {
        // Not a reason to give up on the run. A splash screen, a video surface or an injected
        // overlay can make a swap chain of its own before the game's, and D3D12 requires the
        // ForHwnd path with a command queue, so the one that matters may still be coming.
        if (!g_noDeviceLogged) {
            g_noDeviceLogged = true;
            Log("[reflex] a swap chain with no D3D12 device went past (hr=0x%08X), still waiting for one that has it", (unsigned)hr);
        }
        return;
    }

    // A device we have already turned away. Both reasons for turning one away, the wrong vendor
    // and a driver that refused the mode, are answers about that device and not about the
    // process, so this is a pointer and not a flag.
    if ((IUnknown*)device == g_refusedDevice) {
        Log("[reflex] another swap chain on the device already turned away, still idle");
        device->Release();
        return;
    }

    // The same device with a different swap chain is a swap chain the game replaced, which a
    // resolution or window mode change does without touching the device. Pace the new one. Left
    // alone, the layer would go on believing it was active while pacing a chain that is gone.
    if ((IUnknown*)device == g_device) {
        device->Release();
        if (swapChain != g_swapChain) {
            g_swapChain.store(swapChain);
            Log("[reflex] a newer swap chain on the same device, pacing that one from here");
        }
        return;
    }

    // The vendor check comes before nvapi is touched at all, and it answers for this device's
    // adapter. Only the nvapi lookup answers for the whole process, so only it latches.
    if (g_resolveFailed) { device->Release(); return; }
    if (!RenderAdapterIsNvidia(device)) { g_refusedDevice = (IUnknown*)device; device->Release(); return; }
    if (!g_resolved) {
        if (!Resolve()) { g_resolveFailed = true; device->Release(); return; }
        g_resolved = true;
    }

    // A reset, a driver update or a mode change builds a new device and leaves the old one dead.
    //
    // The dead device is not released here, and that, not the order of the two stores, is what
    // keeps a present on another thread safe. Clearing the flag first only narrows the window: a
    // present that read the flag a moment ago has already passed its check and is going to reach
    // NvAPI_D3D_Sleep with the pointer it read. Dropping the last reference under it would hand
    // the driver freed memory. So it is released at the next rebind instead, which means at most
    // one dead device is held rather than one per reset, which is what F-07 was about.
    if (g_device) {
        Log("[reflex] the game is on a new D3D12 device, rebinding. The one before it paced %llu frames and was refused %llu times.",
            (unsigned long long)g_sleepCalls.load(), (unsigned long long)g_sleepFailures.load());
        g_active.store(false);
        if (g_retiredDevice) g_retiredDevice->Release();
        g_retiredDevice = g_device;
        g_sleepCalls = 0;
        g_sleepFailures = 0;
    }
    g_device.store((IUnknown*)device);
    g_swapChain.store(swapChain);

    NV_SET_SLEEP_MODE_PARAMS p = {};
    p.version = kSleepModeVersion;
    p.bLowLatencyMode = 1;
    p.bLowLatencyBoost = g_cfg.reflexBoost ? 1 : 0;
    p.minimumIntervalUs = 0;     // never a frame rate cap, the mod does not limit
    p.bUseMarkersToOptimize = 0; // a proxy cannot place simulation markers honestly

    int status = g_setSleepMode(g_device.load(), &p);
    if (status != 0) {
        Log("[reflex] NvAPI_D3D_SetSleepMode refused (%d), layer idle. This is normal on a non NVIDIA render adapter.", status);
        // Retired rather than released, for the reason above. A present cannot be pacing this one,
        // since g_active is false all the way through here, but a device that has ever been in
        // g_device follows one rule and not two.
        g_refusedDevice = g_device.load();   // do not ask this one again on its next swap chain
        if (g_retiredDevice) g_retiredDevice->Release();
        g_retiredDevice = g_device.load();
        g_device.store(nullptr);
        g_swapChain.store(nullptr);
        return;
    }
    g_active.store(true);
    Log("[reflex] on: low latency mode %s, no frame rate cap. The frame rate stays unlocked, this only stops the CPU queueing frames further ahead than it can use.",
        g_cfg.reflexBoost ? "+ boost" : "(boost off)");
    ReportStatus("right after enabling it");
}

// What the driver says, rather than what we asked for.
static void ReportStatus(const char* when)
{
    if (!g_active || !g_getSleepStatus) return;
    NV_GET_SLEEP_STATUS_PARAMS s = {};
    s.version = kSleepStatusVersion;
    int status = g_getSleepStatus(g_device.load(), &s);
    if (status != 0) { Log("[reflex] the driver would not report its sleep status (%d)", status); return; }
    Log("[reflex] the driver reports %s: low latency mode %s, fullscreen VRR %s, control panel forcing vsync %s",
        when, s.bLowLatencyMode ? "ON" : "off", s.bFsVrr ? "on" : "off", s.bCplVsyncOn ? "on" : "off");
}

bool Active() { return g_active; }

void OnFrameBegin(IUnknown* swapChain)
{
    if (!g_active.load() || swapChain != g_swapChain.load()) return;
    IUnknown* device = g_device.load();
    if (!device) return;
    int status = g_sleep(device);
    uint64_t n = ++g_sleepCalls;
    if (status != 0) {
        uint64_t bad = ++g_sleepFailures;
        if (bad <= 3) Log("[reflex] NvAPI_D3D_Sleep returned %d", status);
        // A driver that keeps refusing is one we should stop calling every frame.
        if (bad > 100) { g_active = false; Log("[reflex] too many failures, layer off for the rest of this run"); }
    }
    // Once more after a while, because a control panel override or a mode change can turn
    // low latency back off underneath us and the first reading would never show it.
    if (n == 1000) {
        Log("[reflex] running, %llu frames paced, %llu refused", (unsigned long long)n, (unsigned long long)g_sleepFailures.load());
        ReportStatus("after a thousand frames");
    }
}

} // namespace reflex
