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
// Everything is configured by acevo_perf.ini next to this DLL. The only game file replaced is
// dstorage.dll. Everything else the mod writes is its own and goes next to the exe: acevo_perf.log,
// the five developer CSVs when their switches are on, and acevo_bigscreen.texture and
// acevo_uicomponents.css, which the overlay generates from the player's own package and then serves
// in place of the originals. Nothing of the game's is modified in place.
#include "acevo/common.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/core/code_patch.h"
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
    std::wstring switches, exe;
    int args = 0, withValues = 0, others = 0;
    bool firstWord = true;

    // Quote aware, because the exe path has spaces in it on a stock Steam install and splitting on
    // spaces alone turned one argument into five. That made the line report four unnamed arguments
    // for a launch that passed none, and a folder or profile name containing a hyphen came out
    // looking like a switch, which is the one thing this line exists to keep out.
    for (const wchar_t* p = GetCommandLineW(); *p; ) {
        while (*p == L' ' || *p == L'\t') ++p;
        if (!*p) break;
        std::wstring word;
        bool inQuotes = false;
        for (; *p; ++p) {
            if (*p == L'"') { inQuotes = !inQuotes; continue; }
            if (!inQuotes && (*p == L' ' || *p == L'\t')) break;
            word.push_back(*p);
        }
        if (firstWord) {
            firstWord = false;
            // The exe by leaf name. It is the one part of the line that says the game was started
            // through a renamed or wrapping executable, and the folder it sits in is the part
            // that identifies the machine, so the two separate cleanly here.
            exe = PublicPath(word);
            continue;
        }
        if (word.empty()) continue;
        ++args;
        if (word[0] != L'-' && word[0] != L'/') { ++others; continue; }

        // The name is the marker plus letters, digits, underscores and hyphens. Whatever follows
        // is a value in one of the several syntaxes, `=`, `:` or a bare suffix, and it is counted
        // rather than written, since an account name or a session id arrives that way.
        size_t end = 1;
        while (end < word.size() && (iswalnum(word[end]) || word[end] == L'_' || word[end] == L'-')) ++end;
        if (end != word.size()) ++withValues;
        switches += switches.empty() ? L"" : L" ";
        switches += word.substr(0, end);
    }
    Log("command line: %ls, %d argument(s) after it, switches: %ls (%d of them carried a value, not logged), %d other value(s) not logged",
        exe.empty() ? L"(no exe)" : exe.c_str(), args, switches.empty() ? L"(none)" : switches.c_str(), withValues, others);
}

static void OnAttach(HMODULE h)
{
    g_self = h;
    InitDStorageProxy();
    InitFrameStats();

    // Truncation is checked because past MAX_PATH the buffer still comes back null terminated, so
    // g_dir would silently name an ancestor of the real folder: no ini found, and the absolute
    // load of dstorage_orig.dll failing into the message box with nothing saying why.
    std::vector<wchar_t> path(MAX_PATH);
    for (;;) {
        DWORD n = GetModuleFileNameW(h, path.data(), (DWORD)path.size());
        if (n == 0) break;                                       // nothing sensible to do
        if (n < path.size() - 1) break;                          // fits
        if (path.size() >= 32768) break;                         // the longest Windows allows
        path.resize(path.size() * 2);
    }
    g_dir = path.data();
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
    CodePatchingIsNowUnsafe();   // the game gets its own threads from here, see WriteCode
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        OnAttach((HMODULE)hinst);
    } else if (reason == DLL_PROCESS_DETACH) {
        LogDetaching();
        StopLoadSampler();
        StreamerDetach();
        LogClose();
    }
    return TRUE;
}
