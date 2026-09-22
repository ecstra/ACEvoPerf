// ACEvoPerf - performance mod for Assetto Corsa EVO (0.9.x)
//
// Ships as a drop-in dstorage.dll proxy. The game imports DStorageGetFactory
// from dstorage.dll next to the exe, so this DLL is loaded before the game's
// own code runs. Headers live under include/acevo, sources under src, one
// folder per concern. DllMain only wires them:
//   core/       log, ini config, import table and vtable patching
//   dstorage/   the forwarded exports, factory and queue proxies, counters
//   engine/     gflags written into the exe, process priority and timers, texture streamer hooks
//   render/     DXGI factory and swap chain hooks, per frame timing
//   telemetry/  per second CSV, streaming trace
//   overlay/    loose files that shadow package entries
//   ui/         the responsive UI and its parts, the shared Cohtml hooks, the developer UI probe
//
// Everything is configured by acevo_perf.ini next to this DLL and logged to
// acevo_perf.log. No game files other than the replaced dstorage.dll are touched.
#include "acevo/common.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/dstorage/proxy.h"
#include "acevo/engine/flags.h"
#include "acevo/engine/process.h"
#include "acevo/engine/exceptions.h"
#include "acevo/engine/session_leak_fix.h"
#include "acevo/engine/streamer.h"
#include "acevo/render/frame_stats.h"
#include "acevo/render/dxgi_hooks.h"
#include "acevo/overlay/overlay.h"
#include "acevo/telemetry/load_sampler.h"
#include "acevo/telemetry/memory_census.h"
#include "acevo/ui/cohtml_hooks.h"
#include "acevo/ui/responsive_ui.h"
#include "acevo/ui/ui_probe.h"

// Not written out as it stands. The readme tells players to attach this log to a public report,
// and a launcher can put an account name, a session id or a token on the command line. What the
// line is read for is whether the game was started with anything unusual, so the switches are
// listed by name with any value after an equals dropped, and everything else is counted and left
// out. Split on spaces, which mis-splits a quoted path into several words, and that is fine here
// because a path is one of the things being left out anyway.
static void LogCommandLine()
{
    std::wstring line = GetCommandLineW();
    std::wstring switches;
    int words = 0, others = 0;
    size_t at = 0;
    while (at < line.size()) {
        size_t end = line.find(L' ', at);
        if (end == std::wstring::npos) end = line.size();
        std::wstring word = line.substr(at, end - at);
        at = end + 1;
        if (word.empty()) continue;
        if (words++ == 0) continue;            // the exe itself, which PublicPath already covers
        if (word[0] != L'-' && word[0] != L'/') { ++others; continue; }
        size_t eq = word.find(L'=');
        if (eq != std::wstring::npos) word.erase(eq);
        switches += switches.empty() ? L"" : L" ";
        switches += word;
    }
    Log("command line: %d argument(s) after the exe, switches: %ls, %d other value(s) not logged",
        words > 0 ? words - 1 : 0, switches.empty() ? L"(none)" : switches.c_str(), others);
}

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
        LogOpen(lp, g_dir + L"acevo_perf.log");
    }
    Log("ACEvoPerf %s attached (pid %lu). ini=%ls", ACEVO_PERF_VERSION, GetCurrentProcessId(), PublicPath(g_iniPath).c_str());
    // Anything LoadConfig had to correct. It runs before the log file exists, because the log's
    // own path comes out of the ini, so it collects these rather than writing them.
    for (const auto& note : g_cfg.iniNotes) Log("%s", note.c_str());
    char staging[16];
    if (g_cfg.stagingAuto) strcpy_s(staging, "auto"); else _snprintf_s(staging, sizeof staging, _TRUNCATE, "%dMB", g_cfg.stagingMb);
    Log("config: staging=%s minQueueCap=%d submitThreads=%d cpuDecomp=%d bypassIO=%s mappingLayer=%d fileBuffering=%d stats=%d/%ds logRequests=%d | priority=%d powerThrottleOff=%d timer=%dus | flags=%zu | dxgi=%d frameStats=%d timeline=%d frames=%d hitchMs=%d reflex=%d reflexBoost=%d",
        staging, g_cfg.minQueueCapacity, g_cfg.submitThreads, g_cfg.cpuDecompThreads, g_cfg.disableBypassIo ? "disabled" : "enabled",
        g_cfg.forceMappingLayer, g_cfg.forceFileBuffering, g_cfg.stats, g_cfg.statsIntervalS, g_cfg.logRequests,
        g_cfg.priority, g_cfg.disablePowerThrottling, g_cfg.timerResolutionUs, g_cfg.flags.size(),
        g_cfg.dxgiEnabled, g_cfg.frameStats, g_cfg.timeline, g_cfg.frames, g_cfg.hitchMs,
        g_cfg.reflex, g_cfg.reflexBoost);
    LogCommandLine();

    InstallMemoryCensus();
    ApplyProcessTweaks();
    ApplyFlags("early");
    InstallStreamerHooks();
    InstallSessionLeakFix();
    InstallResponsiveUi();
    InstallUiProbe();
    InstallCohtmlHooks();
    InstallDxgiHooks();
    InstallThrowLog();
    overlay::Install();
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        OnAttach((HMODULE)hinst);
    } else if (reason == DLL_PROCESS_DETACH) {
        StopLoadSampler();
        StreamerDetach();
        LogClose();
    }
    return TRUE;
}
