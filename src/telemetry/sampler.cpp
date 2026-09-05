#include "acevo/telemetry/sampler.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/render/frame_stats.h"

namespace {

// columns of the samples CSV, in this order. The system DLLs are split by the nearest
// export name so waiting, locking and memory work do not hide behind one number.
enum Bucket { kGame, kCohtml, kV8, kRenoir, kD3D12, kDriver, kDxgi, kWait, kLock, kHeap, kMemcpy, kSystem, kDStorage, kAudio, kOther, kBucketCount };
const char* const kBucketNames[kBucketCount] = { "game", "cohtml", "v8", "renoir", "d3d12", "driver", "dxgi", "wait", "lock", "heap", "memcpy", "system", "dstorage", "audio", "other" };

struct ModuleRange { uintptr_t base; uintptr_t end; int bucket; bool system; };
ModuleRange g_modules[512];
int g_moduleCount = 0;

// exports of the system DLLs, sorted by address, for the nearest export lookup
struct Export { uintptr_t addr; int bucket; };
std::vector<Export> g_exports;

std::atomic<uint32_t> g_counts[kBucketCount];
std::atomic<uint32_t> g_unknownHits{0};

// game code addresses sampled in the frame under way, cut at every present
const int kRipsPerFrame = 512;
uintptr_t g_rips[2][kRipsPerFrame];
std::atomic<int> g_ripCount[2];
std::atomic<int> g_ripBuffer{0};

// histograms of game code by 64 byte bucket, slow frames against the rest
CRITICAL_SECTION g_histCs;
std::vector<std::pair<uint32_t, uint32_t>> g_slowHist;   // (bucket, count), small and sorted on demand
std::vector<std::pair<uint32_t, uint32_t>> g_fastHist;
uintptr_t g_gameBase = 0;
double g_typicalMs = 0.0;

struct FrameRecord { float t; float ms; uint32_t counts[kBucketCount]; };
std::vector<FrameRecord> g_records;
CRITICAL_SECTION g_recordCs;

std::atomic<DWORD> g_renderThreadId{0};
HANDLE g_renderThread = nullptr;

int BucketForName(const char* name)
{
    if (strstr(name, "NtWaitFor") || strstr(name, "NtDelayExecution") || strstr(name, "NtRemoveIoCompletion") || strstr(name, "RtlWaitOnAddress")
        || strstr(name, "WaitFor") || strstr(name, "SleepConditionVariable") || strstr(name, "SleepEx") || strstr(name, "NtSignalAndWait")
        || strstr(name, "NtYieldExecution")) return kWait;
    if (strstr(name, "CriticalSection") || strstr(name, "SRWLock") || strstr(name, "NtWaitForAlertByThreadId") || strstr(name, "RtlpWaitOn")
        || strstr(name, "AcquireSRW") || strstr(name, "ReleaseSRW")) return kLock;
    if (strstr(name, "Heap") || strstr(name, "malloc") || strstr(name, "free") || strstr(name, "calloc") || strstr(name, "realloc")
        || strstr(name, "operator new") || strstr(name, "operator delete")) return kHeap;
    if (strstr(name, "memcpy") || strstr(name, "memmove") || strstr(name, "memset") || strstr(name, "memcmp") || strstr(name, "RtlCopyMemory")
        || strstr(name, "RtlMoveMemory") || strstr(name, "RtlFillMemory") || strstr(name, "RtlCompareMemory")) return kMemcpy;
    return kSystem;
}

int BucketForModule(const char* name, bool* system)
{
    char lower[MAX_PATH];
    size_t i = 0;
    for (; name[i] && i < MAX_PATH - 1; ++i) lower[i] = (char)tolower((unsigned char)name[i]);
    lower[i] = 0;
    *system = false;
    if (strstr(lower, "assettocorsaevo.exe")) return kGame;
    if (strstr(lower, "cohtml")) return kCohtml;
    if (strstr(lower, "v8")) return kV8;
    if (strstr(lower, "renoir")) return kRenoir;
    if (strstr(lower, "d3d12")) return kD3D12;
    if (strstr(lower, "nvwgf2") || strstr(lower, "nvapi") || strstr(lower, "amdxc") || strstr(lower, "amdvlk") || strstr(lower, "atidxx")) return kDriver;
    if (strstr(lower, "dxgi")) return kDxgi;
    if (strstr(lower, "ntdll") || strstr(lower, "kernelbase") || strstr(lower, "kernel32") || strstr(lower, "ucrtbase") || strstr(lower, "msvcp")
        || strstr(lower, "vcruntime")) { *system = true; return kSystem; }
    if (strstr(lower, "dstorage")) return kDStorage;
    if (strstr(lower, "fmod")) return kAudio;
    return kOther;
}

void CollectExports(HMODULE mod)
{
    BYTE* base = (BYTE*)mod;
    auto dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    auto nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress) return;
    auto exp = (IMAGE_EXPORT_DIRECTORY*)(base + dir.VirtualAddress);
    auto names = (DWORD*)(base + exp->AddressOfNames);
    auto ordinals = (WORD*)(base + exp->AddressOfNameOrdinals);
    auto functions = (DWORD*)(base + exp->AddressOfFunctions);
    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        DWORD rva = functions[ordinals[i]];
        if (rva >= dir.VirtualAddress && rva < dir.VirtualAddress + dir.Size) continue;   // forwarder
        g_exports.push_back({ (uintptr_t)(base + rva), BucketForName((const char*)(base + names[i])) });
    }
}

void RefreshModules()
{
    HMODULE mods[512]; DWORD needed = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof mods, &needed)) return;
    int count = (int)std::min<DWORD>(needed / sizeof(HMODULE), 512);
    ModuleRange table[512]; int n = 0;
    std::vector<Export> exports;
    g_exports.swap(exports);
    g_exports.clear();
    for (int i = 0; i < count; ++i) {
        MODULEINFO info = {};
        char name[MAX_PATH] = {};
        if (!K32GetModuleInformation(GetCurrentProcess(), mods[i], &info, sizeof info)) continue;
        K32GetModuleFileNameExA(GetCurrentProcess(), mods[i], name, MAX_PATH);
        const char* file = strrchr(name, '\\');
        bool system = false;
        int bucket = BucketForModule(file ? file + 1 : name, &system);
        table[n++] = { (uintptr_t)info.lpBaseOfDll, (uintptr_t)info.lpBaseOfDll + info.SizeOfImage, bucket, system };
        if (bucket == kGame) g_gameBase = (uintptr_t)info.lpBaseOfDll;
        if (system) CollectExports(mods[i]);
    }
    std::sort(g_exports.begin(), g_exports.end(), [](const Export& a, const Export& b) { return a.addr < b.addr; });
    memcpy(g_modules, table, sizeof(ModuleRange) * n);
    g_moduleCount = n;
}

int Classify(uintptr_t rip)
{
    for (int i = 0; i < g_moduleCount; ++i) {
        if (rip < g_modules[i].base || rip >= g_modules[i].end) continue;
        if (!g_modules[i].system) return g_modules[i].bucket;
        // nearest export at or before the address
        size_t lo = 0, hi = g_exports.size();
        while (lo < hi) { size_t mid = (lo + hi) / 2; if (g_exports[mid].addr <= rip) lo = mid + 1; else hi = mid; }
        if (lo == 0) return kSystem;
        const Export& e = g_exports[lo - 1];
        return (e.addr >= g_modules[i].base && rip - e.addr < 0x4000) ? e.bucket : kSystem;
    }
    return -1;
}

void Bump(std::vector<std::pair<uint32_t, uint32_t>>& hist, uint32_t key)
{
    for (auto& kv : hist) if (kv.first == key) { kv.second++; return; }
    if (hist.size() < 20000) hist.push_back({ key, 1 });
}

uint32_t CountIn(const std::vector<std::pair<uint32_t, uint32_t>>& hist, uint32_t key)
{
    for (auto& kv : hist) if (kv.first == key) return kv.second;
    return 0;
}

void LogHotSpots()
{
    EnterCriticalSection(&g_histCs);
    std::vector<std::pair<uint32_t, uint32_t>> slow = g_slowHist;
    std::vector<std::pair<uint32_t, uint32_t>> fast = g_fastHist;
    LeaveCriticalSection(&g_histCs);
    if (slow.empty()) return;
    uint32_t slowTotal = 0, fastTotal = 0;
    for (auto& kv : slow) slowTotal += kv.second;
    for (auto& kv : fast) fastTotal += kv.second;
    if (!slowTotal || !fastTotal) return;
    // rank by how much more often a bucket appears in slow frames than its share of the fast frames predicts
    std::sort(slow.begin(), slow.end(), [&](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b) {
        double ea = (double)a.second - (double)CountIn(fast, a.first) * slowTotal / fastTotal;
        double eb = (double)b.second - (double)CountIn(fast, b.first) * slowTotal / fastTotal;
        return ea > eb;
    });
    Log("sampler: game code with the most extra samples in slow frames (rva, slow samples, fast samples scaled to the same total; %u slow, %u fast samples)", slowTotal, fastTotal);
    for (size_t i = 0; i < slow.size() && i < 12; ++i) {
        double scaled = (double)CountIn(fast, slow[i].first) * slowTotal / fastTotal;
        Log("sampler:   rva 0x%06X  slow %5u  fast %7.1f", (unsigned)(slow[i].first << 6), slow[i].second, scaled);
    }
}

void WriteRecords(HANDLE csv)
{
    std::vector<FrameRecord> batch;
    EnterCriticalSection(&g_recordCs);
    batch.swap(g_records);
    LeaveCriticalSection(&g_recordCs);
    if (batch.empty()) return;
    std::string out;
    out.reserve(batch.size() * 80);
    char line[256];
    for (auto& r : batch) {
        int n = _snprintf_s(line, sizeof line, _TRUNCATE, "%.3f,%.2f", r.t, r.ms);
        out.append(line, n);
        for (int b = 0; b < kBucketCount; ++b) {
            n = _snprintf_s(line, sizeof line, _TRUNCATE, ",%u", r.counts[b]);
            out.append(line, n);
        }
        out.append("\r\n");
    }
    DWORD written;
    WriteFile(csv, out.data(), (DWORD)out.size(), &written, nullptr);
}

DWORD WINAPI SamplerThread(void*)
{
    SetThreadDescription(GetCurrentThread(), L"ACEvoPerf sampler");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    RefreshModules();
    Log("sampler: %d modules, %zu system exports for the wait, lock, heap and memcpy split", g_moduleCount, g_exports.size());

    std::wstring path = g_dir + L"acevo_perf_samples.csv";
    HANDLE csv = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (csv != INVALID_HANDLE_VALUE) {
        std::string header = "t_s,frame_ms";
        for (int b = 0; b < kBucketCount; ++b) { header += ","; header += kBucketNames[b]; }
        header += "\r\n";
        DWORD written; WriteFile(csv, header.data(), (DWORD)header.size(), &written, nullptr);
    }

    LARGE_INTEGER qpf; QueryPerformanceFrequency(&qpf);
    const int64_t interval = qpf.QuadPart * (int64_t)g_cfg.sampleUs / 1000000;
    LARGE_INTEGER next; QueryPerformanceCounter(&next);
    int64_t lastFlush = next.QuadPart, lastRefresh = next.QuadPart, lastHot = next.QuadPart;
    CONTEXT ctx;

    for (;;) {
        next.QuadPart += interval;
        LARGE_INTEGER now;
        for (;;) {
            QueryPerformanceCounter(&now);
            if (now.QuadPart >= next.QuadPart) break;
            YieldProcessor();
        }
        if (next.QuadPart < now.QuadPart - interval) next.QuadPart = now.QuadPart;

        HANDLE thread = g_renderThread;
        if (thread) {
            ctx.ContextFlags = CONTEXT_CONTROL;
            if (SuspendThread(thread) != (DWORD)-1) {
                bool ok = GetThreadContext(thread, &ctx) != 0;
                ResumeThread(thread);
                if (ok) {
                    uintptr_t rip = (uintptr_t)ctx.Rip;
                    int bucket = Classify(rip);
                    if (bucket < 0) { g_unknownHits++; bucket = kOther; }
                    g_counts[bucket]++;
                    if (bucket == kGame) {
                        int buf = g_ripBuffer.load();
                        int idx = g_ripCount[buf].fetch_add(1);
                        if (idx < kRipsPerFrame) g_rips[buf][idx] = rip;
                    }
                }
            }
        }

        if (now.QuadPart - lastRefresh > qpf.QuadPart * 5 && g_unknownHits.exchange(0) > 0) {
            RefreshModules();
            lastRefresh = now.QuadPart;
        }
        if (now.QuadPart - lastFlush > qpf.QuadPart) {
            if (csv != INVALID_HANDLE_VALUE) WriteRecords(csv);
            lastFlush = now.QuadPart;
        }
        if (now.QuadPart - lastHot > qpf.QuadPart * 60) {
            LogHotSpots();
            lastHot = now.QuadPart;
        }
    }
}

} // namespace

void SamplerOnPresent(double frameMs)
{
    if (!g_cfg.sampler) return;
    DWORD tid = GetCurrentThreadId();
    if (g_renderThreadId.load() != tid) {
        g_renderThreadId = tid;
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
        HANDLE old = g_renderThread;
        g_renderThread = h;
        if (old) CloseHandle(old);
        Log("sampler: render thread is %lu", tid);
    }

    FrameRecord r;
    r.t = (float)NowSec();
    r.ms = (float)frameMs;
    for (int b = 0; b < kBucketCount; ++b) r.counts[b] = g_counts[b].exchange(0);
    EnterCriticalSection(&g_recordCs);
    if (g_records.size() < 200000) g_records.push_back(r);
    LeaveCriticalSection(&g_recordCs);

    // the game code addresses of the frame that just ended: slow frames and the rest apart
    int finished = g_ripBuffer.load();
    g_ripBuffer.store(finished ^ 1);
    int n = std::min(g_ripCount[finished].exchange(0), kRipsPerFrame);
    if (g_typicalMs <= 0.0) g_typicalMs = frameMs;
    bool slow = frameMs > 1.4 * g_typicalMs;
    g_typicalMs += (frameMs - g_typicalMs) * 0.02;
    if (n > 0 && g_gameBase) {
        EnterCriticalSection(&g_histCs);
        auto& hist = slow ? g_slowHist : g_fastHist;
        for (int i = 0; i < n; ++i) Bump(hist, (uint32_t)((g_rips[finished][i] - g_gameBase) >> 6));
        LeaveCriticalSection(&g_histCs);
    }
}

void StartSampler()
{
    if (!g_cfg.sampler) return;
    static HANDLE thread = nullptr;
    if (thread) return;
    InitializeCriticalSection(&g_recordCs);
    InitializeCriticalSection(&g_histCs);
    thread = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr);
    Log("sampler: started, one sample every %d us, buckets: game, cohtml, v8, renoir, d3d12, driver, dxgi, wait, lock, heap, memcpy, system, dstorage, audio, other", g_cfg.sampleUs);
}
