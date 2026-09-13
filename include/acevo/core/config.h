#pragma once
#include "acevo/common.h"

struct Config {
    // [log]
    bool logEnabled = true;
    std::wstring logFile = L"acevo_perf.log";
    int  hitchMs = 33;          // frames slower than this are logged individually
    // [developer], diagnostics, all off by default
    bool timeline = false;      // acevo_perf_timeline.csv, one line per second
    bool frames = false;        // acevo_perf_frames.csv, one line per presented frame
    bool streamingTrace = false; // acevo_perf_streaming.csv, texture streamer and tile request events
    bool logRequests = false;   // every DirectStorage request in the log
    bool throwLog = false;      // count the game's C++ exceptions by throw site
    bool traceFileIo = false;   // log the game's file I/O on the package
    bool loadSampler = false;   // suspends game threads to read them
    int  loadSampleUs = 1000;
    // [directstorage]
    bool bundledRuntime = true;     // load acevo_perf\dstoragecore.dll instead of the game's older one
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
    int  tileQueuePriority = 99;   // DSTORAGE_PRIORITY value, 99 = leave the game's choice
    // [process]
    int  priority = 1;          // 0 normal, 1 above normal, 2 high
    bool disablePowerThrottling = true;
    int  timerResolutionUs = 500;
    int  gpuPriority = 4;            // D3DKMT scheduling class, -1 = leave alone, 4 = high
    int  workingSetFloorMb = 0;      // 0 = leave alone
    // [engine]
    bool streamerReloadFix = true;   // refuse the texture streamer's drop of a mip it keeps reloading
    // [flags]
    std::vector<std::wstring> flags; // "name=value" or "name"
    // [dxgi]
    bool dxgiEnabled = true;
    bool frameStats = true;
    // [latency]
    bool reflex = true;
    bool reflexBoost = false;
    // [overlay]
    bool overlayEnabled = true;
    std::wstring overlayFolder = L"acevo_mods";
    bool overlayClearXor = true;    // serve override entries as plain data (XOR flag cleared)
    bool fixBigScreens = true;      // serve the trackside flipbook with one mip level, BUG-017
};

extern Config g_cfg;
extern std::wstring g_dir;      // directory containing this DLL (with trailing backslash)
extern std::wstring g_iniPath;
extern HMODULE g_self;

void LoadConfig();
