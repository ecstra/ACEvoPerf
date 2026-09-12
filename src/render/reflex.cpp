// NVIDIA Reflex, see include/acevo/render/reflex.h.
//
// nvapi64.dll exports exactly one symbol, `nvapi_QueryInterface`, and everything else is
// reached by asking it for a numeric id. The ids and the structure below are from NVIDIA's
// own headers (github.com/NVIDIA/nvapi, nvapi_interface.h and nvapi_lite_common.h), not
// guessed: a wrong layout here goes straight into the display driver.
#include "acevo/render/reflex.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"

namespace reflex {

// nvapi_interface.h
static const unsigned kId_Initialize      = 0x0150e828;
static const unsigned kId_Unload          = 0xd22bdd7e;
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
static IUnknown*        g_device = nullptr;
static bool             g_tried = false;
static bool             g_active = false;
static std::atomic<uint64_t> g_sleepCalls{0};
static std::atomic<uint64_t> g_sleepFailures{0};

static void ReportStatus(const char* when);

static bool Resolve()
{
    HMODULE nvapi = LoadLibraryW(L"nvapi64.dll");
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
    if (!g_cfg.reflex || g_tried || !swapChain) return;
    g_tried = true;
    if (!Resolve()) return;

    // Reflex wants the D3D12 device, which the swap chain can hand over.
    IDXGISwapChain* sc = nullptr;
    if (FAILED(swapChain->QueryInterface(__uuidof(IDXGISwapChain), (void**)&sc)) || !sc) return;
    ID3D12Device* device = nullptr;
    HRESULT hr = sc->GetDevice(__uuidof(ID3D12Device), (void**)&device);
    sc->Release();
    if (FAILED(hr) || !device) { Log("[reflex] the swap chain has no D3D12 device (hr=0x%08X), layer idle", (unsigned)hr); return; }
    g_device = device;

    NV_SET_SLEEP_MODE_PARAMS p = {};
    p.version = kSleepModeVersion;
    p.bLowLatencyMode = 1;
    p.bLowLatencyBoost = g_cfg.reflexBoost ? 1 : 0;
    p.minimumIntervalUs = 0;     // never a frame rate cap, the mod does not limit
    p.bUseMarkersToOptimize = 0; // a proxy cannot place simulation markers honestly

    int status = g_setSleepMode(g_device, &p);
    if (status != 0) {
        Log("[reflex] NvAPI_D3D_SetSleepMode refused (%d), layer idle. This is normal on a non NVIDIA render adapter.", status);
        g_device->Release();
        g_device = nullptr;
        return;
    }
    g_active = true;
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
    int status = g_getSleepStatus(g_device, &s);
    if (status != 0) { Log("[reflex] the driver would not report its sleep status (%d)", status); return; }
    Log("[reflex] the driver reports %s: low latency mode %s, fullscreen VRR %s, control panel forcing vsync %s",
        when, s.bLowLatencyMode ? "ON" : "off", s.bFsVrr ? "on" : "off", s.bCplVsyncOn ? "on" : "off");
}

void OnFrameBegin()
{
    if (!g_active) return;
    int status = g_sleep(g_device);
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
