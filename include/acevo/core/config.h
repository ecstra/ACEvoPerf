#pragma once
#include "acevo/common.h"

struct Config {
    // [log]
    bool logEnabled = true;
    std::wstring logFile = L"acevo_perf.log";
    bool timeline = true;       // acevo_perf_timeline.csv, one line per second
    bool frames = true;         // acevo_perf_frames.csv, one line per presented frame
    int  hitchMs = 33;          // frames slower than this are logged individually
    bool throwLog = false;      // count the game's C++ exceptions by throw site
    // [directstorage]
    int  stagingMb = 0;             // 0 = the game's own value, or the auto pick once the card is known
    bool stagingAuto = true;        // staging_buffer_mb=auto, sized from the render adapter's memory
    int  minQueueCapacity = 0;
    int  submitThreads = 0;
    int  cpuDecompThreads = 0;
    bool disableBypassIo = false;
    bool forceMappingLayer = false;
    bool forceFileBuffering = false;
    bool disableGpuDecompression = false;
    bool disableTelemetry = true;
    bool stats = true;
    int  statsIntervalS = 10;
    bool logRequests = false;
    int  tileQueuePriority = 99;   // DSTORAGE_PRIORITY value, 99 = leave the game's choice
    // [process]
    int  priority = 1;          // 0 normal, 1 above normal, 2 high
    bool disablePowerThrottling = true;
    int  timerResolutionUs = 500;
    // [flags]
    std::vector<std::wstring> flags; // "name=value" or "name"
    // [dxgi]
    bool dxgiEnabled = true;
    bool frameStats = true;
    // [overlay]
    bool overlayEnabled = true;
    std::wstring overlayFolder = L"acevo_mods";
    bool overlayClearXor = true;    // serve override entries as plain data (XOR flag cleared)
    bool traceFileIo = false;       // log the game's file I/O on the package
    // [ui]
    int  uiInspectorPort = 0;       // Cohtml DevTools inspector port, 0 = off
    std::wstring uiV8Flags;         // V8 flags applied before Cohtml starts its script engine
    bool uiLayoutThread = false;    // run Cohtml's layout work on a dedicated thread of the mod
    bool uiPatches = true;          // the mod's script patches for the game's UI, run at every page start
};

extern Config g_cfg;
extern std::wstring g_dir;      // directory containing this DLL (with trailing backslash)
extern std::wstring g_iniPath;
extern HMODULE g_self;

void LoadConfig();
