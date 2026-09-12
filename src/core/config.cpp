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
    g_cfg.timeline = IniBool(L"log", L"timeline", false);
    g_cfg.frames = IniBool(L"log", L"frames", false);
    g_cfg.hitchMs = IniInt(L"log", L"hitch_ms", 33);
    g_cfg.throwLog = IniBool(L"log", L"throw_log", false);

    std::wstring staging = IniStr(L"directstorage", L"staging_buffer_mb", L"auto");
    g_cfg.stagingAuto = (staging == L"auto");
    g_cfg.stagingMb = g_cfg.stagingAuto ? 0 : _wtoi(staging.c_str());
    g_cfg.minQueueCapacity = IniInt(L"directstorage", L"min_queue_capacity", 0);
    g_cfg.submitThreads = IniInt(L"directstorage", L"submit_threads", 0);
    g_cfg.cpuDecompThreads = IniInt(L"directstorage", L"cpu_decompression_threads", 0);
    g_cfg.disableBypassIo = IniBool(L"directstorage", L"disable_bypass_io", false);
    g_cfg.forceMappingLayer = IniBool(L"directstorage", L"force_mapping_layer", false);
    g_cfg.forceFileBuffering = IniBool(L"directstorage", L"force_file_buffering", false);
    g_cfg.disableGpuDecompression = IniBool(L"directstorage", L"disable_gpu_decompression", false);
    g_cfg.disableTelemetry = IniBool(L"directstorage", L"disable_telemetry", true);
    g_cfg.stats = IniBool(L"directstorage", L"stats", true);
    g_cfg.statsIntervalS = IniInt(L"directstorage", L"stats_interval_s", 10);
    g_cfg.logRequests = IniBool(L"directstorage", L"log_requests", false);
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

    g_cfg.loadSampler = IniBool(L"profile", L"load_sampler", false);
    g_cfg.loadSampleUs = IniInt(L"profile", L"sample_us", 1000);

    g_cfg.overlayEnabled = IniBool(L"overlay", L"enabled", true);
    g_cfg.overlayFolder = IniStr(L"overlay", L"folder", L"acevo_mods");
    g_cfg.overlayClearXor = IniBool(L"overlay", L"clear_xor_flag", true);
    g_cfg.traceFileIo = IniBool(L"overlay", L"trace_file_io", false);
}
