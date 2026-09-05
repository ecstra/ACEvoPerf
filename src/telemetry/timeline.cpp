#include "acevo/telemetry/timeline.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/render/frame_stats.h"
#include "acevo/render/dxgi_hooks.h"
#include "acevo/dstorage/stats.h"
#include "acevo/engine/input_probe.h"

static HANDLE g_timelineThread = nullptr;

static HANDLE OpenCsv(const wchar_t* name, const char* header)
{
    std::wstring path = g_dir + name;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, header, (DWORD)strlen(header), &w, nullptr); }
    return h;
}

// The adapter with the most dedicated memory is the discrete GPU the game renders on.
static IDXGIAdapter3* FindRenderAdapter()
{
    HMODULE dxgi = GetModuleHandleW(L"dxgi.dll");
    auto createFactory = dxgi ? (PFN_CreateDXGIFactory1)GetProcAddress(dxgi, "CreateDXGIFactory1") : nullptr;
    if (!createFactory) return nullptr;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(createFactory(__uuidof(IDXGIFactory1), (void**)&factory)) || !factory) return nullptr;

    IDXGIAdapter3* best = nullptr; SIZE_T bestMem = 0; wchar_t bestName[128] = L"";
    for (UINT i = 0;; ++i) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(i, &adapter) != S_OK || !adapter) break;
        DXGI_ADAPTER_DESC1 d = {}; adapter->GetDesc1(&d);
        if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && d.DedicatedVideoMemory > bestMem) {
            IDXGIAdapter3* a3 = nullptr;
            if (SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&a3)) && a3) {
                if (best) best->Release();
                best = a3; bestMem = d.DedicatedVideoMemory; wcsncpy_s(bestName, d.Description, 127);
            }
        }
        adapter->Release();
    }
    factory->Release();
    if (best) Log("timeline: VRAM queries on adapter '%ls' (%llu MB dedicated)", bestName, (unsigned long long)(bestMem >> 20));
    return best;
}

static uint64_t FileTimeToU64(const FILETIME& ft) { return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime; }

static DWORD WINAPI TimelineThread(void*)
{
    SetThreadDescription(GetCurrentThread(), L"ACEvoPerf timeline");
    HANDLE csv = g_cfg.timeline ? OpenCsv(L"acevo_perf_timeline.csv",
        "clock,t_s,frames,fps,avg_ms,max_ms,hitch20,hitch_cfg,tile_req,tile_mb,tile_batches,tile_maxbatch,f2m_req,f2m_mb,gpumem_req,gpumem_mb,submits,vram_used_mb,vram_budget_mb,vram_reservable_mb,cpu_proc_pct,cpu_sys_pct,ws_mb,commit_mb,input_polls,input_ms,input_max_ms\r\n") : INVALID_HANDLE_VALUE;
    HANDLE framesCsv = g_cfg.frames ? OpenCsv(L"acevo_perf_frames.csv", "t_s,frame_ms,present_ms,wait_ms,tile_req,f2m_req,gpumem_req\r\n") : INVALID_HANDLE_VALUE;
    IDXGIAdapter3* adapter = FindRenderAdapter();

    SYSTEM_INFO si; GetSystemInfo(&si);
    double cores = (double)si.dwNumberOfProcessors;
    FILETIME c, e, k, u; GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
    uint64_t lastProc = FileTimeToU64(k) + FileTimeToU64(u);
    FILETIME sysIdle, sysKernel, sysUser; GetSystemTimes(&sysIdle, &sysKernel, &sysUser);
    uint64_t lastIdle = FileTimeToU64(sysIdle), lastSysBusy = FileTimeToU64(sysKernel) + FileTimeToU64(sysUser);
    uint64_t lastReq[5] = {}, lastBytes[5] = {}, lastSubmits = 0, lastBatches = 0;
    for (int i = 0; i < 5; ++i) { lastReq[i] = g_reqByDest[i].load(); lastBytes[i] = g_bytesByDest[i].load(); }
    double lastT = NowSec();

    for (;;) {
        Sleep(1000);
        double t = NowSec(); double dt = t - lastT; if (dt <= 0) dt = 1; lastT = t;

        uint64_t frames = g_frames.exchange(0), sumUs = g_frameSumUs.exchange(0), maxUs = g_frameMaxUs.exchange(0);
        uint64_t h20 = g_hitch20.exchange(0), hc = g_hitchCfg.exchange(0);
        g_hitchLogBudget.store(5);

        uint64_t dReq[5], dBytes[5];
        for (int i = 0; i < 5; ++i) {
            uint64_t r = g_reqByDest[i].load(), b = g_bytesByDest[i].load();
            dReq[i] = r - lastReq[i]; dBytes[i] = b - lastBytes[i]; lastReq[i] = r; lastBytes[i] = b;
        }
        uint64_t subs = g_submitsTotal.load(), dSubs = subs - lastSubmits; lastSubmits = subs;
        uint64_t batches = g_tileBatches.load(), dBatches = batches - lastBatches; lastBatches = batches;
        uint64_t maxBatch = g_tileBatchMax.exchange(0);

        DXGI_QUERY_VIDEO_MEMORY_INFO vm = {};
        if (adapter) adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &vm);

        GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u);
        uint64_t proc = FileTimeToU64(k) + FileTimeToU64(u);
        GetSystemTimes(&sysIdle, &sysKernel, &sysUser);
        uint64_t idle = FileTimeToU64(sysIdle), sysBusy = FileTimeToU64(sysKernel) + FileTimeToU64(sysUser);
        double procPct = (double)(proc - lastProc) / (dt * 1e7 * cores) * 100.0;
        double sysTotal = (double)(sysBusy - lastSysBusy);           // kernel time already includes idle time
        double sysPct = sysTotal > 0 ? (1.0 - (double)(idle - lastIdle) / sysTotal) * 100.0 : 0.0;
        lastProc = proc; lastIdle = idle; lastSysBusy = sysBusy;

        PROCESS_MEMORY_COUNTERS_EX pmc = {}; pmc.cb = sizeof pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof pmc);

        uint64_t inputPolls = g_inputCalls.exchange(0), inputUs = g_inputUs.exchange(0), inputMaxUs = g_inputMaxUs.exchange(0);

        SYSTEMTIME st; GetLocalTime(&st);
        char line[1024];
        int n = _snprintf_s(line, sizeof line, _TRUNCATE,
            "%02d:%02d:%02d,%.1f,%llu,%.1f,%.2f,%.1f,%llu,%llu,%llu,%.1f,%llu,%llu,%llu,%.1f,%llu,%.1f,%llu,%llu,%llu,%llu,%.1f,%.1f,%llu,%llu,%llu,%.2f,%.2f\r\n",
            st.wHour, st.wMinute, st.wSecond, t,
            (unsigned long long)frames, frames / dt, frames ? (sumUs / 1000.0) / frames : 0.0, maxUs / 1000.0,
            (unsigned long long)h20, (unsigned long long)hc,
            (unsigned long long)dReq[4], dBytes[4] / 1048576.0, (unsigned long long)dBatches, (unsigned long long)maxBatch,
            (unsigned long long)dReq[0], dBytes[0] / 1048576.0,
            (unsigned long long)(dReq[1] + dReq[2] + dReq[3]), (dBytes[1] + dBytes[2] + dBytes[3]) / 1048576.0,
            (unsigned long long)dSubs,
            (unsigned long long)(vm.CurrentUsage >> 20), (unsigned long long)(vm.Budget >> 20), (unsigned long long)(vm.AvailableForReservation >> 20),
            procPct, sysPct, (unsigned long long)(pmc.WorkingSetSize >> 20), (unsigned long long)(pmc.PrivateUsage >> 20),
            (unsigned long long)inputPolls, inputUs / 1000.0, inputMaxUs / 1000.0);
        if (csv != INVALID_HANDLE_VALUE && n > 0) { DWORD w; WriteFile(csv, line, (DWORD)n, &w, nullptr); }

        if (framesCsv == INVALID_HANDLE_VALUE) continue;
        std::vector<FrameSample> buf;
        EnterCriticalSection(&g_frameCs); buf.swap(g_frameBuf); LeaveCriticalSection(&g_frameCs);
        std::string out; out.reserve(buf.size() * 24);
        char tmp[80];
        for (auto& fr : buf) {
            int m = _snprintf_s(tmp, sizeof tmp, _TRUNCATE, "%.3f,%.2f,%.2f,%.2f,%u,%u,%u\r\n", fr.t, fr.ms, fr.present, fr.wait, fr.tiles, fr.f2m, fr.gpumem);
            out.append(tmp, m);
        }
        if (!out.empty()) { DWORD w; WriteFile(framesCsv, out.data(), (DWORD)out.size(), &w, nullptr); }
    }
}

void StartTimeline()
{
    if (g_timelineThread || (!g_cfg.timeline && !g_cfg.frames)) return;
    g_timelineThread = CreateThread(nullptr, 0, TimelineThread, nullptr, 0, nullptr);
    Log("timeline thread started (timeline=%d frames=%d hitch_ms=%d)", g_cfg.timeline, g_cfg.frames, g_cfg.hitchMs);
}
