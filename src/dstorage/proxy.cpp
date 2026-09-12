#include "acevo/dstorage/proxy.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/dstorage/stats.h"
#include "acevo/dstorage/merge_survey.h"
#include "acevo/engine/flags.h"
#include "acevo/telemetry/timeline.h"
#include "acevo/telemetry/load_sampler.h"
#include "acevo/overlay/overlay.h"

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
static bool g_usingBundledCore = false;

void InitDStorageProxy()
{
    InitializeCriticalSection(&g_realCs);
}

// dstorage_orig.dll is only a forwarder, roughly two hundred kilobytes that find dstoragecore.dll
// and jump into it. The core is the runtime, and the forwarder looks for it by bare name in the
// game executable's folder, where the game keeps its own 1.2.3 copy. Two things follow from that.
// Windows treats a module's base name as its identity, so whoever loads a dstoragecore.dll first
// owns the name and every later load of any path gets that same module back, and this game loads
// its own during start-up, before it ever calls one of our exports. So there is no ordering the
// proxy can win.
//
// The way out is to stop competing for the name. Our copy ships as acevo_dstoragecore.dll, which
// nothing else asks for, and the proxy calls its entry points itself. That is all the forwarder
// was doing: DStorageGetFactory resolves DStorageGetFactoryCore and tail jumps to it with the same
// arguments, SetConfiguration1 and CreateCompressionCodec likewise, and the plain SetConfiguration
// widens the older struct by one field first. Nothing is lost by going straight to the core, and
// the game's own runtime can sit loaded next to ours without either one noticing.
static bool LoadBundledCore()
{
    if (!g_cfg.bundledRuntime) {
        Log("[runtime] bundled_runtime=0, using the game's own DirectStorage runtime");
        return false;
    }

    std::wstring path = g_dir + L"acevo_dstoragecore.dll";
    HMODULE core = LoadLibraryW(path.c_str());
    if (!core) {
        Log("[runtime] cannot load %ls (error %lu), falling back to the game's own runtime. Copy "
            "acevo_dstoragecore.dll from the mod zip next to the exe to get the newer one.",
            path.c_str(), GetLastError());
        return false;
    }

    auto getFactory = (PFN_DStorageGetFactory)GetProcAddress(core, "DStorageGetFactoryCore");
    auto setConfig = (PFN_DStorageSetConfiguration1)GetProcAddress(core, "DStorageSetConfigurationCore");
    auto createCodec = (PFN_DStorageCreateCompressionCodec)GetProcAddress(core, "DStorageCreateCompressionCodecCore");
    if (!getFactory || !setConfig || !createCodec) {
        Log("[runtime] %ls does not offer the core entry points, falling back to the game's own runtime", path.c_str());
        FreeLibrary(core);
        return false;
    }

    g_real = core;
    g_usingBundledCore = true;
    g_realGetFactory = getFactory;
    g_realSetConfiguration1 = setConfig;
    g_realCreateCodec = createCodec;
    g_realSetConfiguration = nullptr;   // the core takes the wider struct only, see the export below

    const UINT32* sdk = (const UINT32*)GetProcAddress(core, "DStorageSDKVersion");
    UINT32 v = sdk ? *sdk : 0;
    Log("[runtime] DirectStorage 1.%u.%u in use, from %ls", v / 100, v % 100, path.c_str());
    if (v < DSTORAGE_SDK_VERSION)
        Log("[runtime] that is older than the 1.%u.%u this mod ships with, so that file is stale",
            (UINT32)DSTORAGE_SDK_VERSION / 100, (UINT32)DSTORAGE_SDK_VERSION % 100);
    return true;
}

static bool EnsureReal()
{
    EnterCriticalSection(&g_realCs);
    if (!g_realTried) {
        g_realTried = true;
        if (LoadBundledCore()) { LeaveCriticalSection(&g_realCs); return true; }
        std::wstring path = g_dir + L"dstorage_orig.dll";
        g_real = LoadLibraryW(path.c_str());
        if (!g_real) {
            DWORD err = GetLastError();
            Log("FATAL: cannot load %ls (error %lu). Reinstall the mod or restore the original dstorage.dll.", path.c_str(), err);
            MessageBoxW(nullptr,
                L"ACEvoPerf: dstorage_orig.dll was not found next to the game executable.\n\n"
                L"Copy all three files from the mod zip (dstorage.dll, dstorage_orig.dll,\n"
                L"acevo_perf.ini) into the game folder, then start the game again.",
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
// IDStorageQueue proxy (statistics + error reporting)
// ---------------------------------------------------------------------------
struct QueueStats {
    std::atomic<uint64_t> requests{0}, bytes{0}, submits{0}, maxReq{0}, sinceSubmit{0};
    std::atomic<uint64_t> byDest[5] = {};
    std::atomic<uint64_t> fromMemory{0}, compressed{0}, gdeflate{0};
    uint64_t lastReportTick = 0;
    uint64_t lastRequests = 0, lastBytes = 0;
};

struct QueueProxy : IDStorageQueue2 {
    IDStorageQueue2* real2 = nullptr;   // may be null if runtime only gives IDStorageQueue
    IDStorageQueue1* real1 = nullptr;
    IDStorageQueue* real = nullptr;
    std::atomic<LONG> ref{1};
    std::string name;
    QueueStats st;
    std::atomic<bool> toldAboutQueue3{false};
    std::atomic<bool> toldAboutUnwrapped{false};
    MergeSurvey merge;   // only touched when [profile] merge_survey=1

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
        if (final && g_cfg.mergeSurvey) merge.Report(name.c_str());
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDStorageQueue) ||
            (real1 && riid == __uuidof(IDStorageQueue1)) || (real2 && riid == __uuidof(IDStorageQueue2))) {
            *ppv = static_cast<IDStorageQueue2*>(this); AddRef(); return S_OK;
        }
        // 1.3 added IDStorageQueue3, and the proxy does not implement it. Handing the real queue
        // over would put the game directly on the runtime, past the overlay's file redirection and
        // past the statistics, so it is declined the way a pre 1.3 runtime declines it. The game is
        // built against 1.2 and never asks. This is here so that a later build cannot quietly slip
        // past the proxy the moment the runtime underneath it gets newer.
        if (riid == __uuidof(IDStorageQueue3)) {
            if (!toldAboutQueue3.exchange(true))
                Log("queue '%s': the game asked for IDStorageQueue3 and was declined, it keeps using the 1.2 "
                    "interface. Nothing is broken, but the mod should grow an EnqueueRequests of its own.", name.c_str());
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        if (!toldAboutUnwrapped.exchange(true))
            Log("queue '%s': handing the real queue over for an interface the proxy does not wrap", name.c_str());
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
            if (g_cfg.mergeSurvey) merge.OnRequest(request);

            DSTORAGE_REQUEST redirected;
            if (OverlayRedirect(request, &redirected)) { real->EnqueueRequest(&redirected); return; }
        }
        real->EnqueueRequest(request);
    }
    void STDMETHODCALLTYPE EnqueueStatus(IDStorageStatusArray* statusArray, UINT32 index) override
    {
        if (g_cfg.mergeSurvey) merge.OnBarrier();
        real->EnqueueStatus(statusArray, index);
    }
    void STDMETHODCALLTYPE EnqueueSignal(ID3D12Fence* fence, UINT64 value) override
    {
        if (g_cfg.mergeSurvey) merge.OnBarrier();
        real->EnqueueSignal(fence, value);
    }
    void STDMETHODCALLTYPE Submit() override
    {
        if (g_cfg.mergeSurvey) merge.OnBarrier();
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
        // The engine names its texture tile queue "GpuUpload File Queue" (observed on 0.9.0).
        bool isTileQueue = d.Name && strstr(d.Name, "GpuUpload File") != nullptr;
        if (isTileQueue && g_cfg.tileQueuePriority != 99) d.Priority = (DSTORAGE_PRIORITY)g_cfg.tileQueuePriority;
        if (g_cfg.minQueueCapacity > 0) {
            int c = g_cfg.minQueueCapacity;
            if (c < DSTORAGE_MIN_QUEUE_CAPACITY) c = DSTORAGE_MIN_QUEUE_CAPACITY;
            if (c > DSTORAGE_MAX_QUEUE_CAPACITY) c = DSTORAGE_MAX_QUEUE_CAPACITY;
            if ((int)d.Capacity < c) d.Capacity = (UINT16)c;
        }
        HRESULT hr = real->CreateQueue(&d, riid, ppv);
        Log("CreateQueue name='%s' source=%s capacity=%u%s priority=%d%s device=%p -> hr=0x%08X",
            d.Name ? d.Name : "", d.SourceType == DSTORAGE_REQUEST_SOURCE_FILE ? "FILE" : "MEMORY",
            d.Capacity, d.Capacity != origCap ? " (raised)" : "", (int)d.Priority,
            d.Priority != desc->Priority ? " (changed)" : "", d.Device, (unsigned)hr);
        if (FAILED(hr) && d.Capacity != origCap) {
            hr = real->CreateQueue(desc, riid, ppv);
            Log("  retry with original capacity %u -> hr=0x%08X", origCap, (unsigned)hr);
        }
        if (SUCCEEDED(hr) && ppv && *ppv && (g_cfg.stats || g_cfg.logRequests || g_cfg.mergeSurvey) &&
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

IDStorageFactory* RealDStorageFactory()
{
    return g_factory ? g_factory->real : nullptr;
}

// Which runtime actually came up, for the path that still goes through the Microsoft forwarder.
// Worth reading back rather than assuming: a newer forwarder paired with an older core works and
// reports no error at all, it just quietly runs the old code, so the only honest answer comes from
// the module that really got loaded. The bundled core path names its own file when it loads it.
static void ReportRuntimeInUse()
{
    if (g_usingBundledCore) return;

    HMODULE core = GetModuleHandleW(L"dstoragecore.dll");
    if (!core) { Log("[runtime] no dstoragecore.dll is loaded, the DirectStorage runtime did not come up"); return; }

    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(core, path, MAX_PATH);
    const UINT32* sdk = (const UINT32*)GetProcAddress(core, "DStorageSDKVersion");
    UINT32 v = sdk ? *sdk : 0;

    Log("[runtime] DirectStorage 1.%u.%u in use, from %ls", v / 100, v % 100, path);
    if (v < DSTORAGE_SDK_VERSION)
        Log("[runtime] that is older than the 1.%u.%u this mod ships, so the game's own runtime is being used. "
            "It works, it is just the old one.", (UINT32)DSTORAGE_SDK_VERSION / 100, (UINT32)DSTORAGE_SDK_VERSION % 100);
}

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
    ReportRuntimeInUse();   // the first call into the forwarder is what loads the core
}

// ---------------------------------------------------------------------------
// Exports (see exports.def)
// ---------------------------------------------------------------------------
extern "C" HRESULT WINAPI DStorageGetFactory(REFIID riid, void** ppv)
{
    if (!EnsureReal() || !g_realGetFactory) return E_FAIL;
    ApplyDStorageConfiguration();
    static bool lateApplied = false;
    if (!lateApplied) { lateApplied = true; ApplyFlags("late"); StartTimeline(); StartLoadSampler(); }
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
    if (!EnsureReal()) return E_FAIL;

    // The core offers one entry point and it takes the wider struct, so when the proxy is talking
    // to the core directly the older one is widened here, the same way the Microsoft forwarder
    // does it: copy the seven fields across and leave ForceFileBuffering off.
    if (!g_realSetConfiguration) {
        if (!g_realSetConfiguration1 || !configuration) return E_FAIL;
        DSTORAGE_CONFIGURATION1 c = {};
        c.NumSubmitThreads = configuration->NumSubmitThreads;
        c.NumBuiltInCpuDecompressionThreads = configuration->NumBuiltInCpuDecompressionThreads;
        c.ForceMappingLayer = configuration->ForceMappingLayer;
        c.DisableBypassIO = configuration->DisableBypassIO;
        c.DisableTelemetry = configuration->DisableTelemetry;
        c.DisableGpuDecompressionMetacommand = configuration->DisableGpuDecompressionMetacommand;
        c.DisableGpuDecompression = configuration->DisableGpuDecompression;
        c.ForceFileBuffering = FALSE;
        Log("game called DStorageSetConfiguration (widened to the 1.1 struct for the core)");
        return g_realSetConfiguration1(&c);
    }

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
