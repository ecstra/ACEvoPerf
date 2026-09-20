#include "acevo/core/config.h"

Config g_cfg;
std::wstring g_dir;
std::wstring g_iniPath;
HMODULE g_self = nullptr;

static void TrimComment(std::wstring& s)
{
    size_t c = s.find(L';');
    if (c != std::wstring::npos && (c == 0 || s[c - 1] == L' ' || s[c - 1] == L'\t')) s.erase(c);
    while (!s.empty() && (s.back() == L' ' || s.back() == L'\t')) s.pop_back();
}
static std::wstring IniStr(const wchar_t* sec, const wchar_t* key, const wchar_t* def)
{
    wchar_t buf[1024];
    GetPrivateProfileStringW(sec, key, def, buf, 1024, g_iniPath.c_str());
    std::wstring s = buf;
    TrimComment(s);
    return s;
}
static int IniInt(const wchar_t* sec, const wchar_t* key, int def)
{
    std::wstring s = IniStr(sec, key, L"");
    if (s.empty()) return def;
    return _wtoi(s.c_str());
}
static void Note(const char* fmt, ...)
{
    char line[512];
    va_list args;
    va_start(args, fmt);
    _vsnprintf_s(line, sizeof line, _TRUNCATE, fmt, args);
    va_end(args);
    g_cfg.iniNotes.push_back(line);
}

// A value the ini can set that has a range outside of which the mod misbehaves rather than doing
// what was asked. Corrected here rather than at the use site, because several of these are read
// from more than one place and a clamp that lives in one of them is a clamp the others do not get.
static int IniIntInRange(const wchar_t* sec, const wchar_t* key, int def, int lo, int hi)
{
    int v = IniInt(sec, key, def);
    if (v >= lo && v <= hi) return v;
    int clamped = v < lo ? lo : hi;
    Note("ini: [%ls] %ls=%d is outside %d to %d, using %d", sec, key, v, lo, hi, clamped);
    return clamped;
}

static bool IniBool(const wchar_t* sec, const wchar_t* key, bool def)
{
    std::wstring s = IniStr(sec, key, L"");
    if (s.empty()) return def;
    for (auto& ch : s) ch = (wchar_t)towlower(ch);
    return s == L"1" || s == L"true" || s == L"yes" || s == L"on";
}

void LoadConfig()
{
    g_cfg.logEnabled = IniBool(L"log", L"enabled", true);
    g_cfg.logFile = IniStr(L"log", L"file", L"acevo_perf.log");
    g_cfg.hitchMs = IniInt(L"log", L"hitch_ms", 33);

    g_cfg.bundledRuntime = IniBool(L"directstorage", L"bundled_runtime", true);
    // Lowercased like every other word valued key, and an empty value means the key is there with
    // nothing after it, which GetPrivateProfileStringW does not treat as absent, so it would not
    // get the default. Both used to fall through to _wtoi, give 0, and leave the staging cap off
    // with the log saying only "staging=0MB".
    std::wstring staging = IniStr(L"directstorage", L"staging_buffer_mb", L"auto");
    for (auto& ch : staging) ch = (wchar_t)towlower(ch);
    if (staging.empty()) staging = L"auto";
    g_cfg.stagingAuto = (staging == L"auto");
    g_cfg.stagingMb = g_cfg.stagingAuto ? 0 : _wtoi(staging.c_str());
    // Megabytes become bytes in a UINT32 at two call sites, so 4096 lands on exactly zero, which
    // DirectStorage reads as "no staging buffer" and fails every request against. The cap is four
    // times the largest size the mod ever picks for itself.
    if (!g_cfg.stagingAuto && (g_cfg.stagingMb < 0 || g_cfg.stagingMb > 1024)) {
        Note("ini: [directstorage] staging_buffer_mb=%d is outside 0 to 1024, using %d", g_cfg.stagingMb, g_cfg.stagingMb < 0 ? 0 : 1024);
        g_cfg.stagingMb = g_cfg.stagingMb < 0 ? 0 : 1024;
    }
    g_cfg.minQueueCapacity = IniInt(L"directstorage", L"min_queue_capacity", 0);
    g_cfg.submitThreads = IniInt(L"directstorage", L"submit_threads", 0);
    g_cfg.cpuDecompThreads = IniInt(L"directstorage", L"cpu_decompression_threads", 0);
    g_cfg.disableBypassIo = IniBool(L"directstorage", L"disable_bypass_io", false);
    g_cfg.forceMappingLayer = IniBool(L"directstorage", L"force_mapping_layer", false);
    g_cfg.forceFileBuffering = IniBool(L"directstorage", L"force_file_buffering", false);
    g_cfg.disableGpuDecompression = IniBool(L"directstorage", L"disable_gpu_decompression", false);
    g_cfg.disableTelemetry = IniBool(L"directstorage", L"disable_telemetry", true);
    g_cfg.stats = IniBool(L"directstorage", L"stats", true);
    // At 0 the elapsed test can never pass, so every Submit would write its own report line, which
    // during a track load is thousands of blocking writes a second on the game's own threads.
    g_cfg.statsIntervalS = IniIntInRange(L"directstorage", L"stats_interval_s", 10, 1, 3600);
    std::wstring tilePr = IniStr(L"directstorage", L"tile_queue_priority", L"unchanged");
    for (auto& ch : tilePr) ch = (wchar_t)towlower(ch);
    g_cfg.tileQueuePriority = (tilePr == L"low") ? DSTORAGE_PRIORITY_LOW : (tilePr == L"normal") ? DSTORAGE_PRIORITY_NORMAL
                            : (tilePr == L"high") ? DSTORAGE_PRIORITY_HIGH : (tilePr == L"realtime") ? DSTORAGE_PRIORITY_REALTIME : 99;

    std::wstring pr = IniStr(L"process", L"priority", L"above_normal");
    for (auto& ch : pr) ch = (wchar_t)towlower(ch);
    g_cfg.priority = (pr == L"high") ? 2 : (pr == L"normal") ? 0 : 1;
    g_cfg.disablePowerThrottling = IniBool(L"process", L"disable_power_throttling", true);
    g_cfg.timerResolutionUs = IniInt(L"process", L"timer_resolution_us", 500);
    std::wstring gp = IniStr(L"process", L"gpu_priority", L"high");
    for (auto& ch : gp) ch = (wchar_t)towlower(ch);
    g_cfg.gpuPriority = (gp == L"unchanged") ? -1 : (gp == L"normal") ? 2 : (gp == L"above_normal") ? 3
                      : (gp == L"realtime") ? 5 : 4;
    g_cfg.workingSetFloorMb = IniInt(L"process", L"working_set_floor_mb", 0);

    g_cfg.streamerReloadFix = IniBool(L"engine", L"streamer_reload_fix", true);
    g_cfg.streamerRankFix = IniBool(L"engine", L"streamer_rank_fix", true);
    g_cfg.streamerPartialLoads = IniBool(L"engine", L"streamer_partial_loads", true);
    g_cfg.responsiveUi = IniBool(L"engine", L"responsive_ui", true);
    g_cfg.sessionLeakFix = IniBool(L"engine", L"session_leak_fix", true);

    {
        std::vector<wchar_t> buf(32768);
        DWORD n = GetPrivateProfileSectionW(L"flags", buf.data(), (DWORD)buf.size(), g_iniPath.c_str());
        const wchar_t* p = buf.data();
        while (n && *p) {
            std::wstring line = p;
            p += line.size() + 1;
            TrimComment(line);
            if (line.empty()) continue;
            size_t eq = line.find(L'=');
            std::wstring key = line.substr(0, eq);
            while (!key.empty() && (key.back() == L' ' || key.back() == L'\t')) key.pop_back();
            if (key.empty()) continue;
            std::wstring val = (eq == std::wstring::npos) ? L"" : line.substr(eq + 1);
            while (!val.empty() && (val.front() == L' ' || val.front() == L'\t')) val.erase(0, 1);
            if (val.empty()) g_cfg.flags.push_back(key);
            else g_cfg.flags.push_back(key + L"=" + val);
        }
    }

    g_cfg.dxgiEnabled = IniBool(L"dxgi", L"enabled", true);
    g_cfg.frameStats = IniBool(L"dxgi", L"frame_stats", true);

    g_cfg.reflex = IniBool(L"latency", L"reflex", true);
    g_cfg.reflexBoost = IniBool(L"latency", L"reflex_boost", false);

    g_cfg.overlayEnabled = IniBool(L"overlay", L"enabled", true);
    // An empty value would make the folder the game folder itself, and the layer would then walk
    // the whole install and offer every file in it as an override, from DllMain.
    g_cfg.overlayFolder = IniStr(L"overlay", L"folder", L"acevo_mods");
    if (g_cfg.overlayFolder.empty()) {
        Note("ini: [overlay] folder is empty, using acevo_mods. An empty value would have made the game folder itself the mods folder.");
        g_cfg.overlayFolder = L"acevo_mods";
    }
    g_cfg.overlayClearXor = IniBool(L"overlay", L"clear_xor_flag", true);
    g_cfg.fixBigScreens = IniBool(L"overlay", L"fix_big_screens", true);

    // Diagnostics live in one section so it is clear they are for testing, all off by default.
    g_cfg.timeline = IniBool(L"developer", L"timeline", false);
    g_cfg.frames = IniBool(L"developer", L"frames", false);
    g_cfg.streamingTrace = IniBool(L"developer", L"streaming_trace", false);
    g_cfg.logRequests = IniBool(L"developer", L"log_requests", false);
    g_cfg.throwLog = IniBool(L"developer", L"throw_log", false);
    g_cfg.traceFileIo = IniBool(L"developer", L"trace_file_io", false);
    g_cfg.loadSampler = IniBool(L"developer", L"load_sampler", false);
    // At 0 the sampler's wait returns at once and a highest priority thread suspends and resumes
    // the game's busiest threads with no gap, which needs the process killed to get out of.
    g_cfg.loadSampleUs = IniIntInRange(L"developer", L"sample_us", 1000, 100, 1000000);
    g_cfg.uiProbe = IniBool(L"developer", L"ui_probe", false);
    g_cfg.hudScheduleTest = IniBool(L"developer", L"hud_schedule_test", false);
    g_cfg.memoryCensus = IniBool(L"developer", L"memory_census", false);
}
