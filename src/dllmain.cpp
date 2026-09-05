// ACEvoPerf - performance mod for Assetto Corsa EVO (0.9.x)
//
// Ships as a drop-in dstorage.dll proxy. The game imports DStorageGetFactory
// from dstorage.dll next to the exe, so this DLL is loaded before the game's
// own code runs. It:
//   1. Forwards every DirectStorage export to the real runtime (dstorage_orig.dll)
//      and applies DStorageSetConfiguration1 + SetStagingBufferSize tuning.
//   2. Wraps IDStorageFactory / IDStorageQueue to log what the engine streams.
//   3. Sets engine flags (gflags FLAGS_* variables) listed in acevo_perf.ini by
//      locating their storage inside the game exe at runtime.
//   4. Applies process-level tweaks (priority class, power throttling, timer).
//   5. Hooks the DXGI swap chain for frame timing (and optionally a latency cap).
//   6. Writes a per-second telemetry CSV (fps, hitches, streaming, VRAM, CPU).
//
// Everything is configured by acevo_perf.ini next to this DLL and logged to
// acevo_perf.log. No game files other than the renamed dstorage.dll are touched.

#define PSAPI_VERSION 2
#include <windows.h>
#include <psapi.h>
#include <cctype>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdio>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include "dstorage.h"

#define ACEVO_PERF_VERSION "0.2.0"

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
static HANDLE g_log = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION g_logCs;

static void Log(const char* fmt, ...)
{
    if (g_log == INVALID_HANDLE_VALUE) return;
    char buf[4096];
    SYSTEMTIME st; GetLocalTime(&st);
    int n = _snprintf_s(buf, sizeof buf, _TRUNCATE, "[%02d:%02d:%02d.%03d] ",
                        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list ap; va_start(ap, fmt);
    int m = vsnprintf(buf + n, sizeof buf - n - 3, fmt, ap);
    va_end(ap);
    if (m < 0) m = 0;
    if (m > (int)(sizeof buf - n - 3)) m = (int)(sizeof buf - n - 3);
    n += m;
    buf[n++] = '\r'; buf[n++] = '\n';
    EnterCriticalSection(&g_logCs);
    DWORD w; WriteFile(g_log, buf, (DWORD)n, &w, nullptr);
    LeaveCriticalSection(&g_logCs);
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
struct Config {
    // [log]
    bool logEnabled = true;
    std::wstring logFile = L"acevo_perf.log";
    bool timeline = true;       // acevo_perf_timeline.csv, one line per second
    bool frames = true;         // acevo_perf_frames.csv, one line per presented frame
    int  hitchMs = 33;          // frames slower than this are logged individually
    // [directstorage]
    int  stagingMb = 256;
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
    // [process]
    int  priority = 1;          // 0 normal, 1 above normal, 2 high
    bool disablePowerThrottling = true;
    int  timerResolutionUs = 500;
    // [flags]
    std::vector<std::wstring> flags; // "name=value" or "name"
    // [dxgi]
    bool dxgiEnabled = true;
    bool frameStats = true;
    int  maxFrameLatency = 0;
};

static Config g_cfg;
static std::wstring g_dir;      // directory containing this DLL (with trailing backslash)
static std::wstring g_iniPath;
static HMODULE g_self = nullptr;

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

static void LoadConfig()
{
    g_cfg.logEnabled = IniBool(L"log", L"enabled", true);
    g_cfg.logFile = IniStr(L"log", L"file", L"acevo_perf.log");
    g_cfg.timeline = IniBool(L"log", L"timeline", true);
    g_cfg.frames = IniBool(L"log", L"frames", true);
    g_cfg.hitchMs = IniInt(L"log", L"hitch_ms", 33);

    g_cfg.stagingMb = IniInt(L"directstorage", L"staging_buffer_mb", 256);
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

    std::wstring pr = IniStr(L"process", L"priority", L"above_normal");
    for (auto& ch : pr) ch = (wchar_t)towlower(ch);
    g_cfg.priority = (pr == L"high") ? 2 : (pr == L"normal") ? 0 : 1;
    g_cfg.disablePowerThrottling = IniBool(L"process", L"disable_power_throttling", true);
    g_cfg.timerResolutionUs = IniInt(L"process", L"timer_resolution_us", 500);

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
    g_cfg.maxFrameLatency = IniInt(L"dxgi", L"max_frame_latency", 0);
}

static void ApplyFlags(const char* phase);
static void StartTimeline();

// ---------------------------------------------------------------------------
// Real DirectStorage runtime
// ---------------------------------------------------------------------------
typedef HRESULT (WINAPI *PFN_DStorageGetFactory)(REFIID, void**);
typedef HRESULT (WINAPI *PFN_DStorageSetConfiguration)(DSTORAGE_CONFIGURATION const*);
typedef HRESULT (WINAPI *PFN_DStorageSetConfiguration1)(DSTORAGE_CONFIGURATION1 const*);
typedef HRESULT (WINAPI *PFN_DStorageCreateCompressionCodec)(DSTORAGE_COMPRESSION_FORMAT, UINT32, REFIID, void**);

static HMODULE g_real = nullptr;
static PFN_DStorageGetFactory g_realGetFactory = nullptr;
static PFN_DStorageSetConfiguration g_realSetConfiguration = nullptr;
static PFN_DStorageSetConfiguration1 g_realSetConfiguration1 = nullptr;
static PFN_DStorageCreateCompressionCodec g_realCreateCodec = nullptr;
static CRITICAL_SECTION g_realCs;
static bool g_realTried = false;

static bool EnsureReal()
{
    EnterCriticalSection(&g_realCs);
    if (!g_realTried) {
        g_realTried = true;
        std::wstring path = g_dir + L"dstorage_orig.dll";
        g_real = LoadLibraryW(path.c_str());
        if (!g_real) {
            DWORD err = GetLastError();
            Log("FATAL: cannot load %ls (error %lu). Reinstall the mod or restore the original dstorage.dll.", path.c_str(), err);
            MessageBoxW(nullptr,
                L"ACEvoPerf: dstorage_orig.dll was not found next to the game executable.\n\n"
                L"The mod needs the original Microsoft DirectStorage DLL renamed to dstorage_orig.dll.\n"
                L"Run install.ps1 again or restore the original dstorage.dll.",
                L"ACEvoPerf", MB_ICONERROR | MB_OK);
        } else {
            g_realGetFactory = (PFN_DStorageGetFactory)GetProcAddress(g_real, "DStorageGetFactory");
            g_realSetConfiguration = (PFN_DStorageSetConfiguration)GetProcAddress(g_real, "DStorageSetConfiguration");
            g_realSetConfiguration1 = (PFN_DStorageSetConfiguration1)GetProcAddress(g_real, "DStorageSetConfiguration1");
            g_realCreateCodec = (PFN_DStorageCreateCompressionCodec)GetProcAddress(g_real, "DStorageCreateCompressionCodec");
            Log("Loaded real runtime %ls (GetFactory=%p SetConfiguration1=%p)", path.c_str(), g_realGetFactory, g_realSetConfiguration1);
        }
    }
    LeaveCriticalSection(&g_realCs);
    return g_real != nullptr;
}

// ---------------------------------------------------------------------------
// Streaming statistics
// ---------------------------------------------------------------------------
static const char* DestName(UINT64 d)
{
    switch (d) {
    case DSTORAGE_REQUEST_DESTINATION_MEMORY: return "MEMORY";
    case DSTORAGE_REQUEST_DESTINATION_BUFFER: return "BUFFER";
    case DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION: return "TEXTURE_REGION";
    case DSTORAGE_REQUEST_DESTINATION_MULTIPLE_SUBRESOURCES: return "MULTI_SUBRES";
    case DSTORAGE_REQUEST_DESTINATION_TILES: return "TILES";
    default: return "?";
    }
}

// process-wide counters read by the timeline thread
static std::atomic<uint64_t> g_reqByDest[5], g_bytesByDest[5], g_submitsTotal{0};
static std::atomic<uint64_t> g_tileBatches{0}, g_tileBatchMax{0};

struct QueueStats {
    std::atomic<uint64_t> requests{0}, bytes{0}, submits{0}, maxReq{0}, sinceSubmit{0};
    std::atomic<uint64_t> byDest[5] = {};
    std::atomic<uint64_t> fromMemory{0}, compressed{0}, gdeflate{0};
    uint64_t lastReportTick = 0;
    uint64_t lastRequests = 0, lastBytes = 0;
};

// ---------------------------------------------------------------------------
// IDStorageQueue proxy (statistics + error reporting)
// ---------------------------------------------------------------------------
struct QueueProxy : IDStorageQueue2 {
    IDStorageQueue2* real2 = nullptr;   // may be null if runtime only gives IDStorageQueue
    IDStorageQueue1* real1 = nullptr;
    IDStorageQueue* real = nullptr;
    std::atomic<LONG> ref{1};
    std::string name;
    QueueStats st;

    QueueProxy(IDStorageQueue* q, const char* n) : real(q), name(n ? n : "(unnamed)")
    {
        q->QueryInterface(__uuidof(IDStorageQueue1), (void**)&real1);
        q->QueryInterface(__uuidof(IDStorageQueue2), (void**)&real2);
        st.lastReportTick = GetTickCount64();
    }
    ~QueueProxy()
    {
        Report(true);
        if (real2) real2->Release();
        if (real1) real1->Release();
        if (real) real->Release();
    }
    void Report(bool final)
    {
        uint64_t now = GetTickCount64();
        uint64_t dt = now - st.lastReportTick;
        if (!final && dt < (uint64_t)g_cfg.statsIntervalS * 1000ull) return;
        uint64_t r = st.requests.load(), b = st.bytes.load();
        uint64_t dr = r - st.lastRequests, db = b - st.lastBytes;
        double secs = dt / 1000.0; if (secs <= 0) secs = 1;
        Log("[stats] queue '%s'%s: total %llu req / %.1f MB (max req %llu KB) | last %.0fs: %llu req, %.1f MB/s | dest MEM=%llu BUF=%llu TEX=%llu MULTI=%llu TILES=%llu | fromMem=%llu compressed=%llu gdeflate=%llu submits=%llu",
            name.c_str(), final ? " (final)" : "", (unsigned long long)r, b / 1048576.0, (unsigned long long)(st.maxReq.load() / 1024),
            secs, (unsigned long long)dr, (db / 1048576.0) / secs,
            (unsigned long long)st.byDest[0].load(), (unsigned long long)st.byDest[1].load(), (unsigned long long)st.byDest[2].load(),
            (unsigned long long)st.byDest[3].load(), (unsigned long long)st.byDest[4].load(),
            (unsigned long long)st.fromMemory.load(), (unsigned long long)st.compressed.load(), (unsigned long long)st.gdeflate.load(),
            (unsigned long long)st.submits.load());
        st.lastReportTick = now; st.lastRequests = r; st.lastBytes = b;
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDStorageQueue) ||
            (real1 && riid == __uuidof(IDStorageQueue1)) || (real2 && riid == __uuidof(IDStorageQueue2))) {
            *ppv = static_cast<IDStorageQueue2*>(this); AddRef(); return S_OK;
        }
        return real->QueryInterface(riid, ppv);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)++ref; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG r = --ref;
        if (r == 0) delete this;
        return (ULONG)r;
    }
    // IDStorageQueue
    void STDMETHODCALLTYPE EnqueueRequest(const DSTORAGE_REQUEST* request) override
    {
        if (request) {
            uint64_t sz = (request->Options.SourceType == DSTORAGE_REQUEST_SOURCE_MEMORY) ? request->Source.Memory.Size : request->Source.File.Size;
            st.requests++; st.bytes += sz;
            uint64_t prev = st.maxReq.load();
            while (sz > prev && !st.maxReq.compare_exchange_weak(prev, sz)) {}
            UINT64 dt = request->Options.DestinationType;
            if (dt < 5) { st.byDest[dt]++; g_reqByDest[dt]++; g_bytesByDest[dt] += sz; }
            st.sinceSubmit++;
            if (request->Options.SourceType == DSTORAGE_REQUEST_SOURCE_MEMORY) st.fromMemory++;
            if (request->Options.CompressionFormat != DSTORAGE_COMPRESSION_FORMAT_NONE) st.compressed++;
            if (request->Options.CompressionFormat == DSTORAGE_COMPRESSION_FORMAT_GDEFLATE) st.gdeflate++;
            if (g_cfg.logRequests) {
                if (request->Options.SourceType == DSTORAGE_REQUEST_SOURCE_MEMORY)
                    Log("[req] '%s' MEM->%s size=%u uncomp=%u comp=%u name=%s", name.c_str(), DestName(dt), request->Source.Memory.Size, request->UncompressedSize, (unsigned)request->Options.CompressionFormat, request->Name ? request->Name : "");
                else
                    Log("[req] '%s' FILE off=%llu size=%u ->%s uncomp=%u comp=%u name=%s", name.c_str(), (unsigned long long)request->Source.File.Offset, request->Source.File.Size, DestName(dt), request->UncompressedSize, (unsigned)request->Options.CompressionFormat, request->Name ? request->Name : "");
            }
        }
        real->EnqueueRequest(request);
    }
    void STDMETHODCALLTYPE EnqueueStatus(IDStorageStatusArray* statusArray, UINT32 index) override { real->EnqueueStatus(statusArray, index); }
    void STDMETHODCALLTYPE EnqueueSignal(ID3D12Fence* fence, UINT64 value) override { real->EnqueueSignal(fence, value); }
    void STDMETHODCALLTYPE Submit() override
    {
        st.submits++; g_submitsTotal++;
        uint64_t batch = st.sinceSubmit.exchange(0);
        bool isTileQueue = st.byDest[DSTORAGE_REQUEST_DESTINATION_TILES].load() > 0;
        if (batch && isTileQueue) {
            g_tileBatches++;
            uint64_t prev = g_tileBatchMax.load();
            while (batch > prev && !g_tileBatchMax.compare_exchange_weak(prev, batch)) {}
        }
        real->Submit();
        Report(false);
    }
    void STDMETHODCALLTYPE CancelRequestsWithTag(UINT64 mask, UINT64 value) override { real->CancelRequestsWithTag(mask, value); }
    void STDMETHODCALLTYPE Close() override { Log("queue '%s' closed", name.c_str()); Report(true); real->Close(); }
    HANDLE STDMETHODCALLTYPE GetErrorEvent() override { return real->GetErrorEvent(); }
    void STDMETHODCALLTYPE RetrieveErrorRecord(DSTORAGE_ERROR_RECORD* record) override
    {
        real->RetrieveErrorRecord(record);
        if (record && record->FailureCount)
            Log("[error] queue '%s': %u failures, first hr=0x%08X cmd=%d", name.c_str(), record->FailureCount, (unsigned)record->FirstFailure.HResult, (int)record->FirstFailure.CommandType);
    }
    void STDMETHODCALLTYPE Query(DSTORAGE_QUEUE_INFO* info) override { real->Query(info); }
    // IDStorageQueue1
    void STDMETHODCALLTYPE EnqueueSetEvent(HANDLE handle) override { if (real1) real1->EnqueueSetEvent(handle); }
    // IDStorageQueue2
    DSTORAGE_COMPRESSION_SUPPORT STDMETHODCALLTYPE GetCompressionSupport(DSTORAGE_COMPRESSION_FORMAT format) override
    {
        return real2 ? real2->GetCompressionSupport(format) : DSTORAGE_COMPRESSION_SUPPORT_NONE;
    }
};

// ---------------------------------------------------------------------------
// IDStorageFactory proxy
// ---------------------------------------------------------------------------
struct FactoryProxy : IDStorageFactory {
    IDStorageFactory* real;
    std::atomic<LONG> ref{1};
    explicit FactoryProxy(IDStorageFactory* r) : real(r) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDStorageFactory)) { *ppv = this; AddRef(); return S_OK; }
        return real->QueryInterface(riid, ppv);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)++ref; }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = --ref; if (r == 0) { real->Release(); delete this; } return (ULONG)r; }

    HRESULT STDMETHODCALLTYPE CreateQueue(const DSTORAGE_QUEUE_DESC* desc, REFIID riid, void** ppv) override
    {
        DSTORAGE_QUEUE_DESC d = *desc;
        UINT16 origCap = d.Capacity;
        if (g_cfg.minQueueCapacity > 0) {
            int c = g_cfg.minQueueCapacity;
            if (c < DSTORAGE_MIN_QUEUE_CAPACITY) c = DSTORAGE_MIN_QUEUE_CAPACITY;
            if (c > DSTORAGE_MAX_QUEUE_CAPACITY) c = DSTORAGE_MAX_QUEUE_CAPACITY;
            if ((int)d.Capacity < c) d.Capacity = (UINT16)c;
        }
        HRESULT hr = real->CreateQueue(&d, riid, ppv);
        Log("CreateQueue name='%s' source=%s capacity=%u%s priority=%d device=%p -> hr=0x%08X",
            d.Name ? d.Name : "", d.SourceType == DSTORAGE_REQUEST_SOURCE_FILE ? "FILE" : "MEMORY",
            d.Capacity, d.Capacity != origCap ? " (raised)" : "", (int)d.Priority, d.Device, (unsigned)hr);
        if (FAILED(hr) && d.Capacity != origCap) {
            hr = real->CreateQueue(desc, riid, ppv);
            Log("  retry with original capacity %u -> hr=0x%08X", origCap, (unsigned)hr);
        }
        if (SUCCEEDED(hr) && ppv && *ppv && (g_cfg.stats || g_cfg.logRequests) &&
            (riid == __uuidof(IDStorageQueue) || riid == __uuidof(IDStorageQueue1) || riid == __uuidof(IDStorageQueue2))) {
            IDStorageQueue* q = nullptr;
            if (SUCCEEDED(((IUnknown*)*ppv)->QueryInterface(__uuidof(IDStorageQueue), (void**)&q))) {
                ((IUnknown*)*ppv)->Release();
                *ppv = static_cast<IDStorageQueue2*>(new QueueProxy(q, d.Name));
            }
        }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE OpenFile(const WCHAR* path, REFIID riid, void** ppv) override
    {
        HRESULT hr = real->OpenFile(path, riid, ppv);
        Log("OpenFile '%ls' -> hr=0x%08X", path ? path : L"", (unsigned)hr);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE CreateStatusArray(UINT32 capacity, PCSTR name, REFIID riid, void** ppv) override
    {
        HRESULT hr = real->CreateStatusArray(capacity, name, riid, ppv);
        Log("CreateStatusArray capacity=%u name='%s' -> hr=0x%08X", capacity, name ? name : "", (unsigned)hr);
        return hr;
    }
    void STDMETHODCALLTYPE SetDebugFlags(UINT32 flags) override { Log("SetDebugFlags 0x%X", flags); real->SetDebugFlags(flags); }
    HRESULT STDMETHODCALLTYPE SetStagingBufferSize(UINT32 size) override
    {
        UINT32 want = size;
        if (g_cfg.stagingMb > 0) want = (UINT32)g_cfg.stagingMb * 1048576u;
        HRESULT hr = real->SetStagingBufferSize(want);
        Log("game called SetStagingBufferSize(%u MB) -> applied %u MB, hr=0x%08X", size / 1048576u, want / 1048576u, (unsigned)hr);
        if (FAILED(hr) && want != size) { hr = real->SetStagingBufferSize(size); Log("  fallback to game value -> hr=0x%08X", (unsigned)hr); }
        return hr;
    }
};

static FactoryProxy* g_factory = nullptr;
static bool g_configApplied = false;

static void ApplyDStorageConfiguration()
{
    if (g_configApplied) return;
    g_configApplied = true;
    DSTORAGE_CONFIGURATION1 c = {};
    c.NumSubmitThreads = (UINT32)(g_cfg.submitThreads > 0 ? g_cfg.submitThreads : 0);
    c.NumBuiltInCpuDecompressionThreads = g_cfg.cpuDecompThreads;
    c.ForceMappingLayer = g_cfg.forceMappingLayer;
    c.DisableBypassIO = g_cfg.disableBypassIo;
    c.DisableTelemetry = g_cfg.disableTelemetry;
    c.DisableGpuDecompressionMetacommand = FALSE;
    c.DisableGpuDecompression = g_cfg.disableGpuDecompression;
    c.ForceFileBuffering = g_cfg.forceFileBuffering;
    HRESULT hr = E_NOTIMPL;
    if (g_realSetConfiguration1) hr = g_realSetConfiguration1(&c);
    else if (g_realSetConfiguration) hr = g_realSetConfiguration((DSTORAGE_CONFIGURATION*)&c);
    Log("DStorageSetConfiguration1: submitThreads=%u cpuDecompThreads=%d forceMappingLayer=%d disableBypassIO=%d disableTelemetry=%d disableGpuDecomp=%d forceFileBuffering=%d -> hr=0x%08X",
        c.NumSubmitThreads, c.NumBuiltInCpuDecompressionThreads, c.ForceMappingLayer, c.DisableBypassIO, c.DisableTelemetry, c.DisableGpuDecompression, c.ForceFileBuffering, (unsigned)hr);
}

extern "C" HRESULT WINAPI DStorageGetFactory(REFIID riid, void** ppv)
{
    if (!EnsureReal() || !g_realGetFactory) return E_FAIL;
    ApplyDStorageConfiguration();
    static bool lateApplied = false;
    if (!lateApplied) { lateApplied = true; ApplyFlags("late"); StartTimeline(); }
    if (riid != __uuidof(IDStorageFactory)) {
        HRESULT hr = g_realGetFactory(riid, ppv);
        Log("DStorageGetFactory(non-IDStorageFactory riid) -> hr=0x%08X", (unsigned)hr);
        return hr;
    }
    EnterCriticalSection(&g_realCs);
    if (!g_factory) {
        IDStorageFactory* f = nullptr;
        HRESULT hr = g_realGetFactory(__uuidof(IDStorageFactory), (void**)&f);
        Log("DStorageGetFactory -> hr=0x%08X factory=%p", (unsigned)hr, f);
        if (FAILED(hr) || !f) { LeaveCriticalSection(&g_realCs); return hr; }
        if (g_cfg.stagingMb > 0) {
            HRESULT h2 = f->SetStagingBufferSize((UINT32)g_cfg.stagingMb * 1048576u);
            Log("SetStagingBufferSize(%d MB) at factory creation -> hr=0x%08X", g_cfg.stagingMb, (unsigned)h2);
        }
        g_factory = new FactoryProxy(f);
    }
    g_factory->AddRef();
    *ppv = static_cast<IDStorageFactory*>(g_factory);
    LeaveCriticalSection(&g_realCs);
    return S_OK;
}

extern "C" HRESULT WINAPI DStorageSetConfiguration(DSTORAGE_CONFIGURATION const* configuration)
{
    if (!EnsureReal() || !g_realSetConfiguration) return E_FAIL;
    Log("game called DStorageSetConfiguration (forwarded unchanged)");
    return g_realSetConfiguration(configuration);
}

extern "C" HRESULT WINAPI DStorageSetConfiguration1(DSTORAGE_CONFIGURATION1 const* configuration)
{
    if (!EnsureReal() || !g_realSetConfiguration1) return E_FAIL;
    Log("game called DStorageSetConfiguration1 (forwarded unchanged)");
    return g_realSetConfiguration1(configuration);
}

extern "C" HRESULT WINAPI DStorageCreateCompressionCodec(DSTORAGE_COMPRESSION_FORMAT format, UINT32 numThreads, REFIID riid, void** ppv)
{
    if (!EnsureReal() || !g_realCreateCodec) return E_FAIL;
    Log("game called DStorageCreateCompressionCodec(format=%u threads=%u)", (unsigned)format, numThreads);
    return g_realCreateCodec(format, numThreads, riid, ppv);
}

// ---------------------------------------------------------------------------
// IAT patching helper
// ---------------------------------------------------------------------------
static int PatchIatByAddress(HMODULE mod, void* target, void* replacement)
{
    if (!mod || !target) return 0;
    int patched = 0;
    __try {
        BYTE* base = (BYTE*)mod;
        auto dos = (IMAGE_DOS_HEADER*)base;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
        auto nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
        auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dir.VirtualAddress || !dir.Size) return 0;
        auto desc = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress);
        for (; desc->Name; ++desc) {
            if (!desc->FirstThunk) continue;
            auto thunk = (IMAGE_THUNK_DATA64*)(base + desc->FirstThunk);
            for (; thunk->u1.Function; ++thunk) {
                if ((void*)thunk->u1.Function == target) {
                    DWORD old = 0;
                    if (VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &old)) {
                        thunk->u1.Function = (ULONGLONG)replacement;
                        VirtualProtect(&thunk->u1.Function, sizeof(void*), old, &old);
                        ++patched;
                    }
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return patched;
    }
    return patched;
}

// ---------------------------------------------------------------------------
// Engine flags (gflags) - located by scanning the game exe for the
// FlagRegisterer call sites and written directly into FLAGS_* storage.
//
// The release build only parses a small whitelist of single-dash switches
// from the command line, so the generic gflags parser never sees anything.
// Every DEFINE_bool/int32/double/string still registers itself at startup via
//   FlagRegisterer(name, help, __FILE__, &FLAGS_x, &FLAGS_nonox)
// (rdx = name, r8 = help, r9 = file, [rsp+20h]/[rsp+28h] = storages), which is
// a stable, recognisable code pattern. We find those call sites, group them by
// constructor (one instantiation per type) and poke the storage.
// ---------------------------------------------------------------------------
struct FlagInfo {
    std::string name;
    int type = -1;                  // 0 bool, 1 int32, 2 double, 3 string
    std::vector<BYTE*> storages;    // FLAGS_x and FLAGS_nonox (whichever are writable get set)
    std::string file;
};
static std::vector<FlagInfo> g_flags;
static bool g_flagsScanned = false;

struct Range { BYTE* lo = nullptr; BYTE* hi = nullptr; bool has(const void* p) const { return (BYTE*)p >= lo && (BYTE*)p < hi; } };

static bool ReadCString(const BYTE* p, const Range& r, std::string& out, size_t maxLen)
{
    out.clear();
    for (size_t i = 0; i < maxLen; ++i) {
        if (!r.has(p + i)) return false;
        BYTE c = p[i];
        if (c == 0) return !out.empty();
        if (c < 9 || c >= 127) return false;
        out.push_back((char)c);
    }
    return false;
}

static void ScanFlags()
{
    if (g_flagsScanned) return;
    g_flagsScanned = true;
    ULONGLONG t0 = GetTickCount64();
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    auto dos = (IMAGE_DOS_HEADER*)base;
    auto nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    Range text, rdata, image;
    image.lo = base; image.hi = base + nt->OptionalHeader.SizeOfImage;
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        Range r; r.lo = base + sec[i].VirtualAddress; r.hi = r.lo + sec[i].Misc.VirtualSize;
        if (memcmp(sec[i].Name, ".text", 6) == 0) text = r;
        else if (memcmp(sec[i].Name, ".rdata", 7) == 0) rdata = r;
    }
    if (!text.lo || !rdata.lo) { Log("flags: .text/.rdata not found"); return; }

    // pass 1: lea r9,[rip+x] -> "<...>.cpp" followed by a call within 90 bytes -> candidate ctor
    struct Ctor { BYTE* addr; int hits; int type; };
    std::vector<Ctor> ctors;
    std::string s;
    BYTE* p = text.lo; BYTE* end = text.hi - 8;
    for (; p < end; ++p) {
        if (p[0] != 0x4C || p[1] != 0x8D || p[2] != 0x0D) continue;
        BYTE* tgt = p + 7 + *(int32_t*)(p + 3);
        if (!rdata.has(tgt) || !ReadCString(tgt, rdata, s, 300)) continue;
        size_t n = s.size();
        if (!((n > 4 && s.compare(n - 4, 4, ".cpp") == 0) || (n > 3 && s.compare(n - 3, 3, ".cc") == 0))) continue;
        for (BYTE* q = p + 7; q < p + 97 && q < end; ++q) {
            if (*q != 0xE8) continue;
            BYTE* ct = q + 5 + *(int32_t*)(q + 1);
            if (!text.has(ct)) break;
            bool found = false;
            for (auto& c : ctors) if (c.addr == ct) { c.hits++; found = true; break; }
            if (!found) ctors.push_back({ ct, 1, -1 });
            break;
        }
    }
    std::vector<Ctor> good;
    for (auto& c : ctors) if (c.hits >= 3) good.push_back(c);
    if (good.empty()) { Log("flags: no FlagRegisterer candidates found"); return; }

    // pass 2: every call to a candidate ctor -> name (last lea rdx) + storages (lea rax; mov [rsp+20h/28h],rax)
    struct Site { BYTE* ctor; std::string name; std::vector<BYTE*> st; std::string file; };
    std::vector<Site> sites;
    for (p = text.lo; p < end; ++p) {
        if (*p != 0xE8) continue;
        BYTE* ct = p + 5 + *(int32_t*)(p + 1);
        bool isCtor = false;
        for (auto& c : good) if (c.addr == ct) { isCtor = true; break; }
        if (!isCtor) continue;
        Site site; site.ctor = ct;
        BYTE* lo = p - 220; if (lo < text.lo) lo = text.lo;
        BYTE* nameLea = nullptr; BYTE* fileLea = nullptr;
        BYTE* curStorage = nullptr; BYTE* defStorage = nullptr;   // last [rsp+20h] / [rsp+28h] before the call
        for (BYTE* q = lo; q + 7 <= p; ++q) {
            if (q[1] != 0x8D) continue;
            if (q[0] == 0x48 && q[2] == 0x15) nameLea = q;                    // lea rdx
            else if (q[0] == 0x4C && q[2] == 0x0D) fileLea = q;               // lea r9
            else if (q[0] == 0x48 && q[2] == 0x05) {                          // lea rax
                BYTE* tgt = q + 7 + *(int32_t*)(q + 3);
                // must be followed (within 8 bytes) by mov [rsp+20h|28h],rax
                for (BYTE* m = q + 7; m < q + 15 && m + 5 <= p; ++m)
                    if (m[0] == 0x48 && m[1] == 0x89 && m[2] == 0x44 && m[3] == 0x24 && (m[4] == 0x20 || m[4] == 0x28)) {
                        if (image.has(tgt)) { if (m[4] == 0x20) curStorage = tgt; else defStorage = tgt; }
                        break;
                    }
            }
        }
        if (curStorage) site.st.push_back(curStorage);
        if (defStorage && defStorage != curStorage) site.st.push_back(defStorage);
        if (!nameLea) continue;
        BYTE* nt2 = nameLea + 7 + *(int32_t*)(nameLea + 3);
        if (!rdata.has(nt2) || !ReadCString(nt2, rdata, site.name, 120)) continue;
        bool ident = true;
        for (char c : site.name) if (!(isalnum((unsigned char)c) || c == '_')) { ident = false; break; }
        if (!ident || site.st.empty()) continue;
        if (fileLea) { BYTE* ft = fileLea + 7 + *(int32_t*)(fileLea + 3); if (rdata.has(ft)) ReadCString(ft, rdata, site.file, 300); }
        sites.push_back(site);
    }

    // type per ctor from well-known flags
    struct Known { const char* n; int t; } known[] = { {"no_intro",0}, {"vr",0}, {"dumplevel",1}, {"opponent_count",1}, {"ipd_mm",2}, {"track_limit_safezone",2}, {"log_file",3}, {"startup_scene",3} };
    for (auto& c : good)
        for (auto& si : sites) if (si.ctor == c.addr)
            for (auto& k : known) if (si.name == k.n) c.type = k.t;
    for (auto& si : sites) {
        FlagInfo fi; fi.name = si.name; fi.storages = si.st;
        for (auto& c : good) if (c.addr == si.ctor) fi.type = c.type;
        size_t sl = si.file.find_last_of('\\');
        fi.file = (sl == std::string::npos) ? si.file : si.file.substr(sl + 1);
        bool dup = false;
        for (auto& f : g_flags) if (f.name == fi.name) { dup = true; break; }
        if (!dup) g_flags.push_back(fi);
    }
    int typed = 0; for (auto& f : g_flags) if (f.type >= 0) typed++;
    Log("flags: scanned exe in %llu ms: %zu ctor candidates, %zu flags (%d typed)", GetTickCount64() - t0, good.size(), g_flags.size(), typed);
}

static bool Writable(const void* p)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof mbi)) return false;
    DWORD pr = mbi.Protect & 0xFF;
    return mbi.State == MEM_COMMIT && (pr == PAGE_READWRITE || pr == PAGE_WRITECOPY || pr == PAGE_EXECUTE_READWRITE || pr == PAGE_EXECUTE_WRITECOPY);
}

static void ApplyFlags(const char* phase)
{
    if (g_cfg.flags.empty()) return;
    ScanFlags();
    for (auto& f : g_cfg.flags) {
        size_t eq = f.find(L'=');
        std::wstring wname = f.substr(0, eq);
        std::wstring wval = (eq == std::wstring::npos) ? L"true" : f.substr(eq + 1);
        std::string name(wname.begin(), wname.end());
        std::string val(wval.begin(), wval.end());
        FlagInfo* fi = nullptr;
        for (auto& x : g_flags) if (x.name == name) { fi = &x; break; }
        if (!fi) { Log("flag %s: not found in this game build (ignored)", name.c_str()); continue; }
        if (fi->type == 3) { Log("flag %s: string flags are not supported (ignored)", name.c_str()); continue; }
        if (fi->type < 0) { Log("flag %s: unknown type (ignored)", name.c_str()); continue; }
        int written = 0;
        for (BYTE* st : fi->storages) {
            if (!Writable(st)) continue;
            if (fi->type == 0) {
                std::string v = val; for (auto& ch : v) ch = (char)tolower((unsigned char)ch);
                bool b = (v == "1" || v == "true" || v == "yes" || v == "on" || v == "t");
                bool old = *(bool*)st; *(bool*)st = b;
                Log("flag %s = %s (bool, was %s) @%p [%s, %s]", name.c_str(), b ? "true" : "false", old ? "true" : "false", st, fi->file.c_str(), phase);
            } else if (fi->type == 1) {
                int v = atoi(val.c_str()); int old = *(int*)st; *(int*)st = v;
                Log("flag %s = %d (int32, was %d) @%p [%s, %s]", name.c_str(), v, old, st, fi->file.c_str(), phase);
            } else if (fi->type == 2) {
                double v = atof(val.c_str()); double old = *(double*)st; *(double*)st = v;
                Log("flag %s = %g (double, was %g) @%p [%s, %s]", name.c_str(), v, old, st, fi->file.c_str(), phase);
            }
            ++written;
        }
        if (!written) Log("flag %s: no writable storage found (ignored)", name.c_str());
    }
}

// ---------------------------------------------------------------------------
// Process tweaks
// ---------------------------------------------------------------------------
typedef LONG (NTAPI *PFN_NtSetTimerResolution)(ULONG, BOOLEAN, PULONG);

static void ApplyProcessTweaks()
{
    if (g_cfg.priority == 2) { Log("priority class: HIGH -> %d", SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)); }
    else if (g_cfg.priority == 1) { Log("priority class: ABOVE_NORMAL -> %d", SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS)); }
    else Log("priority class: unchanged");

    if (g_cfg.disablePowerThrottling) {
        PROCESS_POWER_THROTTLING_STATE pts = {};
        pts.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        pts.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
        pts.StateMask = 0; // both off: no EcoQoS, always honour our timer resolution
        BOOL ok = SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &pts, sizeof pts);
        Log("power throttling (EcoQoS) disabled, timer resolution always honoured -> %d (err %lu)", ok, ok ? 0 : GetLastError());
    }
    if (g_cfg.timerResolutionUs > 0) {
        HMODULE nt = GetModuleHandleW(L"ntdll.dll");
        auto fn = nt ? (PFN_NtSetTimerResolution)GetProcAddress(nt, "NtSetTimerResolution") : nullptr;
        if (fn) {
            ULONG actual = 0;
            LONG s = fn((ULONG)g_cfg.timerResolutionUs * 10, TRUE, &actual);
            Log("timer resolution requested %d us -> actual %lu us (status 0x%08X)", g_cfg.timerResolutionUs, actual / 10, (unsigned)s);
        }
    }
}

// ---------------------------------------------------------------------------
// Frame statistics (IDXGISwapChain::Present hook)
// ---------------------------------------------------------------------------
static LARGE_INTEGER g_qpf = {};
static LARGE_INTEGER g_qpcStart = {};
static int64_t g_lastPresentQpc = 0;
static std::atomic<uint64_t> g_frames{0}, g_frameSumUs{0}, g_frameMaxUs{0}, g_hitch20{0}, g_hitchCfg{0};
static std::atomic<int> g_hitchLogBudget{5};
static CRITICAL_SECTION g_frameCs;
static std::vector<std::pair<float, float>> g_frameBuf;   // (seconds since attach, frame ms)
static uint64_t g_hitchSnap[5] = {};
static UINT g_lastSyncInterval = 0xFFFFFFFF;

static double NowSec()
{
    LARGE_INTEGER n; QueryPerformanceCounter(&n);
    return (double)(n.QuadPart - g_qpcStart.QuadPart) / (double)g_qpf.QuadPart;
}

static void OnPresent(UINT syncInterval)
{
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    int64_t last = g_lastPresentQpc;
    g_lastPresentQpc = now.QuadPart;
    if (syncInterval != g_lastSyncInterval) {
        g_lastSyncInterval = syncInterval;
        Log("Present sync interval = %u (%s)", syncInterval, syncInterval ? "vsync on" : "vsync off");
    }
    if (!last) return;

    double ms = (double)(now.QuadPart - last) * 1000.0 / (double)g_qpf.QuadPart;
    if (ms > 2000.0) return;                     // alt-tab or loading screen pause, not a frame
    uint64_t us = (uint64_t)(ms * 1000.0);
    g_frames++; g_frameSumUs += us;
    uint64_t prev = g_frameMaxUs.load();
    while (us > prev && !g_frameMaxUs.compare_exchange_weak(prev, us)) {}
    if (ms > 20.0) g_hitch20++;

    if (ms > (double)g_cfg.hitchMs) {
        g_hitchCfg++;
        if (g_hitchLogBudget.fetch_sub(1) > 0) {
            uint64_t cur[5]; for (int i = 0; i < 5; ++i) cur[i] = g_reqByDest[i].load();
            Log("[hitch] %.1f ms frame at t=%.2fs | since previous hitch: tiles %llu req, file->mem %llu req, mem->gpu %llu req",
                ms, NowSec(), (unsigned long long)(cur[4] - g_hitchSnap[4]), (unsigned long long)(cur[0] - g_hitchSnap[0]),
                (unsigned long long)(cur[1] + cur[2] - g_hitchSnap[1] - g_hitchSnap[2]));
            for (int i = 0; i < 5; ++i) g_hitchSnap[i] = cur[i];
        }
    }

    if (g_cfg.frames) {
        EnterCriticalSection(&g_frameCs);
        if (g_frameBuf.size() < 200000) g_frameBuf.emplace_back((float)NowSec(), (float)ms);
        LeaveCriticalSection(&g_frameCs);
    }
}

typedef HRESULT (STDMETHODCALLTYPE *PFN_Present)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT (STDMETHODCALLTYPE *PFN_Present1)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
static PFN_Present g_origPresent = nullptr;
static PFN_Present1 g_origPresent1 = nullptr;

static HRESULT STDMETHODCALLTYPE Hook_Present(IDXGISwapChain* self, UINT sync, UINT flags)
{
    if (!(flags & DXGI_PRESENT_TEST)) OnPresent(sync);
    return g_origPresent(self, sync, flags);
}
static HRESULT STDMETHODCALLTYPE Hook_Present1(IDXGISwapChain1* self, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS* pp)
{
    if (!(flags & DXGI_PRESENT_TEST)) OnPresent(sync);
    return g_origPresent1(self, sync, flags, pp);
}

static void HookVtableSlot(void** vt, int idx, void* hook, void** orig, const char* what)
{
    if (*orig) return;
    DWORD old = 0;
    if (!VirtualProtect(&vt[idx], sizeof(void*), PAGE_READWRITE, &old)) return;
    *orig = vt[idx];
    vt[idx] = hook;
    VirtualProtect(&vt[idx], sizeof(void*), old, &old);
    Log("DXGI: hooked %s", what);
}

static void HookSwapChain(IUnknown* sc)
{
    if (!sc || !g_cfg.frameStats) return;
    IDXGISwapChain1* sc1 = nullptr;
    if (FAILED(sc->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&sc1)) || !sc1) return;
    void** vt = *(void***)sc1;
    HookVtableSlot(vt, 8, (void*)&Hook_Present, (void**)&g_origPresent, "IDXGISwapChain::Present");
    HookVtableSlot(vt, 22, (void*)&Hook_Present1, (void**)&g_origPresent1, "IDXGISwapChain1::Present1");
    DXGI_SWAP_CHAIN_DESC1 d = {};
    if (SUCCEEDED(sc1->GetDesc1(&d)))
        Log("swap chain: %ux%u fmt=%u buffers=%u swapEffect=%u flags=0x%X scaling=%u", d.Width, d.Height, (unsigned)d.Format, d.BufferCount, (unsigned)d.SwapEffect, d.Flags, (unsigned)d.Scaling);
    sc1->Release();
}

// ---------------------------------------------------------------------------
// DXGI factory hooks (swap chain creation)
// ---------------------------------------------------------------------------
typedef HRESULT (WINAPI *PFN_CreateDXGIFactory1)(REFIID, void**);
typedef HRESULT (WINAPI *PFN_CreateDXGIFactory2)(UINT, REFIID, void**);
typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateSwapChainForHwnd)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateSwapChain)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
static PFN_CreateDXGIFactory1 g_realCDF1 = nullptr;
static PFN_CreateDXGIFactory2 g_realCDF2 = nullptr;
static PFN_CreateSwapChainForHwnd g_origCSCFH = nullptr;
static PFN_CreateSwapChain g_origCSC = nullptr;

static HRESULT STDMETHODCALLTYPE Hook_CreateSwapChainForHwnd(IDXGIFactory2* self, IUnknown* device, HWND hwnd,
    const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fs, IDXGIOutput* out, IDXGISwapChain1** pp)
{
    DXGI_SWAP_CHAIN_DESC1 d = *desc;
    bool flip = (d.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD || d.SwapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL);
    bool tweak = g_cfg.maxFrameLatency > 0 && flip;
    if (tweak) d.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    Log("CreateSwapChainForHwnd %ux%u fmt=%u buffers=%u swapEffect=%u flags=0x%X%s", d.Width, d.Height, (unsigned)d.Format, d.BufferCount, (unsigned)d.SwapEffect, d.Flags, tweak ? " (+waitable)" : "");
    HRESULT hr = g_origCSCFH(self, device, hwnd, &d, fs, out, pp);
    if (FAILED(hr) && tweak) {
        Log("  failed (0x%08X); retrying with original flags", (unsigned)hr);
        hr = g_origCSCFH(self, device, hwnd, desc, fs, out, pp);
        tweak = false;
    }
    if (FAILED(hr) || !pp || !*pp) return hr;

    if (tweak) {
        IDXGISwapChain2* sc2 = nullptr;
        if (SUCCEEDED((*pp)->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&sc2))) {
            HRESULT h2 = sc2->SetMaximumFrameLatency((UINT)g_cfg.maxFrameLatency);
            Log("  SetMaximumFrameLatency(%d) -> hr=0x%08X", g_cfg.maxFrameLatency, (unsigned)h2);
            sc2->Release();
        }
    }
    HookSwapChain(*pp);
    return hr;
}
static HRESULT STDMETHODCALLTYPE Hook_CreateSwapChain(IDXGIFactory* self, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** pp)
{
    Log("CreateSwapChain (legacy) %ux%u buffers=%u swapEffect=%u flags=0x%X", desc->BufferDesc.Width, desc->BufferDesc.Height, desc->BufferCount, (unsigned)desc->SwapEffect, desc->Flags);
    HRESULT hr = g_origCSC(self, device, desc, pp);
    if (SUCCEEDED(hr) && pp && *pp) HookSwapChain(*pp);
    return hr;
}

static void HookFactoryVtable(void* factory)
{
    if (!factory) return;
    IDXGIFactory2* f2 = nullptr;
    if (FAILED(((IUnknown*)factory)->QueryInterface(__uuidof(IDXGIFactory2), (void**)&f2)) || !f2) return;
    void** vt = *(void***)f2;
    HookVtableSlot(vt, 15, (void*)&Hook_CreateSwapChainForHwnd, (void**)&g_origCSCFH, "IDXGIFactory2::CreateSwapChainForHwnd");
    HookVtableSlot(vt, 10, (void*)&Hook_CreateSwapChain, (void**)&g_origCSC, "IDXGIFactory::CreateSwapChain");
    f2->Release();
}
static HRESULT WINAPI Hook_CreateDXGIFactory1(REFIID riid, void** ppv)
{
    HRESULT hr = g_realCDF1(riid, ppv);
    if (SUCCEEDED(hr) && ppv) HookFactoryVtable(*ppv);
    return hr;
}
static HRESULT WINAPI Hook_CreateDXGIFactory2(UINT flags, REFIID riid, void** ppv)
{
    HRESULT hr = g_realCDF2(flags, riid, ppv);
    if (SUCCEEDED(hr) && ppv) HookFactoryVtable(*ppv);
    return hr;
}
static void InstallDxgiHooks()
{
    if (!g_cfg.dxgiEnabled) return;
    HMODULE dxgi = GetModuleHandleW(L"dxgi.dll");
    if (!dxgi) { Log("DXGI: dxgi.dll not loaded at attach time; hook skipped"); return; }
    g_realCDF1 = (PFN_CreateDXGIFactory1)GetProcAddress(dxgi, "CreateDXGIFactory1");
    g_realCDF2 = (PFN_CreateDXGIFactory2)GetProcAddress(dxgi, "CreateDXGIFactory2");
    HMODULE exe = GetModuleHandleW(nullptr);
    int a = PatchIatByAddress(exe, (void*)g_realCDF1, (void*)&Hook_CreateDXGIFactory1);
    int b = PatchIatByAddress(exe, (void*)g_realCDF2, (void*)&Hook_CreateDXGIFactory2);
    Log("DXGI: IAT hooks in game exe: CreateDXGIFactory1=%d CreateDXGIFactory2=%d (frame_stats=%d max_frame_latency=%d)", a, b, g_cfg.frameStats, g_cfg.maxFrameLatency);
}

// ---------------------------------------------------------------------------
// Timeline thread: one CSV line per second plus a per-frame CSV
// ---------------------------------------------------------------------------
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
        "clock,t_s,frames,fps,avg_ms,max_ms,hitch20,hitch_cfg,tile_req,tile_mb,tile_batches,tile_maxbatch,f2m_req,f2m_mb,gpumem_req,gpumem_mb,submits,vram_used_mb,vram_budget_mb,vram_reservable_mb,cpu_proc_pct,cpu_sys_pct,ws_mb,commit_mb\r\n") : INVALID_HANDLE_VALUE;
    HANDLE framesCsv = g_cfg.frames ? OpenCsv(L"acevo_perf_frames.csv", "t_s,frame_ms\r\n") : INVALID_HANDLE_VALUE;
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

        SYSTEMTIME st; GetLocalTime(&st);
        char line[1024];
        int n = _snprintf_s(line, sizeof line, _TRUNCATE,
            "%02d:%02d:%02d,%.1f,%llu,%.1f,%.2f,%.1f,%llu,%llu,%llu,%.1f,%llu,%llu,%llu,%.1f,%llu,%.1f,%llu,%llu,%llu,%llu,%.1f,%.1f,%llu,%llu\r\n",
            st.wHour, st.wMinute, st.wSecond, t,
            (unsigned long long)frames, frames / dt, frames ? (sumUs / 1000.0) / frames : 0.0, maxUs / 1000.0,
            (unsigned long long)h20, (unsigned long long)hc,
            (unsigned long long)dReq[4], dBytes[4] / 1048576.0, (unsigned long long)dBatches, (unsigned long long)maxBatch,
            (unsigned long long)dReq[0], dBytes[0] / 1048576.0,
            (unsigned long long)(dReq[1] + dReq[2] + dReq[3]), (dBytes[1] + dBytes[2] + dBytes[3]) / 1048576.0,
            (unsigned long long)dSubs,
            (unsigned long long)(vm.CurrentUsage >> 20), (unsigned long long)(vm.Budget >> 20), (unsigned long long)(vm.AvailableForReservation >> 20),
            procPct, sysPct, (unsigned long long)(pmc.WorkingSetSize >> 20), (unsigned long long)(pmc.PrivateUsage >> 20));
        if (csv != INVALID_HANDLE_VALUE && n > 0) { DWORD w; WriteFile(csv, line, (DWORD)n, &w, nullptr); }

        if (framesCsv == INVALID_HANDLE_VALUE) continue;
        std::vector<std::pair<float, float>> buf;
        EnterCriticalSection(&g_frameCs); buf.swap(g_frameBuf); LeaveCriticalSection(&g_frameCs);
        std::string out; out.reserve(buf.size() * 16);
        char tmp[64];
        for (auto& fr : buf) { int m = _snprintf_s(tmp, sizeof tmp, _TRUNCATE, "%.3f,%.2f\r\n", fr.first, fr.second); out.append(tmp, m); }
        if (!out.empty()) { DWORD w; WriteFile(framesCsv, out.data(), (DWORD)out.size(), &w, nullptr); }
    }
}

static void StartTimeline()
{
    if (g_timelineThread || (!g_cfg.timeline && !g_cfg.frames)) return;
    g_timelineThread = CreateThread(nullptr, 0, TimelineThread, nullptr, 0, nullptr);
    Log("timeline thread started (timeline=%d frames=%d hitch_ms=%d)", g_cfg.timeline, g_cfg.frames, g_cfg.hitchMs);
}

// ---------------------------------------------------------------------------
// DllMain
// ---------------------------------------------------------------------------
static void OnAttach(HMODULE h)
{
    g_self = h;
    InitializeCriticalSection(&g_logCs);
    InitializeCriticalSection(&g_realCs);
    InitializeCriticalSection(&g_frameCs);
    QueryPerformanceFrequency(&g_qpf);
    QueryPerformanceCounter(&g_qpcStart);

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
        g_log = CreateFileW(lp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    Log("ACEvoPerf %s attached (pid %lu). ini=%ls", ACEVO_PERF_VERSION, GetCurrentProcessId(), g_iniPath.c_str());
    Log("config: staging=%dMB minQueueCap=%d submitThreads=%d cpuDecomp=%d bypassIO=%s mappingLayer=%d fileBuffering=%d stats=%d/%ds logRequests=%d | priority=%d powerThrottleOff=%d timer=%dus | flags=%zu | dxgi=%d frameStats=%d latency=%d timeline=%d frames=%d hitchMs=%d",
        g_cfg.stagingMb, g_cfg.minQueueCapacity, g_cfg.submitThreads, g_cfg.cpuDecompThreads, g_cfg.disableBypassIo ? "disabled" : "enabled",
        g_cfg.forceMappingLayer, g_cfg.forceFileBuffering, g_cfg.stats, g_cfg.statsIntervalS, g_cfg.logRequests,
        g_cfg.priority, g_cfg.disablePowerThrottling, g_cfg.timerResolutionUs, g_cfg.flags.size(),
        g_cfg.dxgiEnabled, g_cfg.frameStats, g_cfg.maxFrameLatency, g_cfg.timeline, g_cfg.frames, g_cfg.hitchMs);
    Log("command line: %ls", GetCommandLineW());

    ApplyProcessTweaks();
    ApplyFlags("early");
    InstallDxgiHooks();
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        OnAttach((HMODULE)hinst);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_log != INVALID_HANDLE_VALUE) { Log("detached"); CloseHandle(g_log); g_log = INVALID_HANDLE_VALUE; }
    }
    return TRUE;
}
