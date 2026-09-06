// ACEvoPerf - performance mod for Assetto Corsa EVO (0.9.x)
//
// Ships as a drop-in dstorage.dll proxy. The game imports DStorageGetFactory
// from dstorage.dll next to the exe, so this DLL is loaded before the game's
// own code runs. Headers live under include/acevo, sources under src, one
// folder per concern. DllMain only wires them:
//   core/       log, ini config, import table and vtable patching
//   dstorage/   the forwarded exports, factory and queue proxies, counters
//   engine/     gflags written into the exe, process priority and timers
//   render/     DXGI factory and swap chain hooks, per frame timing
//   telemetry/  per second CSV
//   overlay/    loose files that shadow package entries
//
// Everything is configured by acevo_perf.ini next to this DLL and logged to
// acevo_perf.log. No game files other than the replaced dstorage.dll are touched.
#include "acevo/common.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/dstorage/proxy.h"
#include "acevo/engine/flags.h"
#include "acevo/engine/process.h"
#include "acevo/render/frame_stats.h"
#include "acevo/render/dxgi_hooks.h"
#include "acevo/overlay/overlay.h"

static void OnAttach(HMODULE h)
{
    g_self = h;
    InitDStorageProxy();
    InitFrameStats();

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(h, path, MAX_PATH);
    g_dir = path;
    size_t s = g_dir.find_last_of(L'\\');
    g_dir = (s == std::wstring::npos) ? L"" : g_dir.substr(0, s + 1);
    g_iniPath = g_dir + L"acevo_perf.ini";
    LoadConfig();

    if (g_cfg.logEnabled) {
        std::wstring lp = g_cfg.logFile;
        if (lp.find(L':') == std::wstring::npos && lp.rfind(L"\\\\", 0) != 0) lp = g_dir + lp;
        LogOpen(lp);
    }
    Log("ACEvoPerf %s attached (pid %lu). ini=%ls", ACEVO_PERF_VERSION, GetCurrentProcessId(), g_iniPath.c_str());
    Log("config: staging=%dMB minQueueCap=%d submitThreads=%d cpuDecomp=%d bypassIO=%s mappingLayer=%d fileBuffering=%d stats=%d/%ds logRequests=%d | priority=%d powerThrottleOff=%d timer=%dus | flags=%zu | dxgi=%d frameStats=%d timeline=%d frames=%d hitchMs=%d",
        g_cfg.stagingMb, g_cfg.minQueueCapacity, g_cfg.submitThreads, g_cfg.cpuDecompThreads, g_cfg.disableBypassIo ? "disabled" : "enabled",
        g_cfg.forceMappingLayer, g_cfg.forceFileBuffering, g_cfg.stats, g_cfg.statsIntervalS, g_cfg.logRequests,
        g_cfg.priority, g_cfg.disablePowerThrottling, g_cfg.timerResolutionUs, g_cfg.flags.size(),
        g_cfg.dxgiEnabled, g_cfg.frameStats, g_cfg.timeline, g_cfg.frames, g_cfg.hitchMs);
    Log("command line: %ls", GetCommandLineW());

    ApplyProcessTweaks();
    ApplyFlags("early");
    InstallDxgiHooks();
    overlay::Install();
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        OnAttach((HMODULE)hinst);
    } else if (reason == DLL_PROCESS_DETACH) {
        LogClose();
    }
    return TRUE;
}
