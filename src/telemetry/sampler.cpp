#include "acevo/telemetry/sampler.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/render/frame_stats.h"
#include <unordered_map>

namespace {

// columns of the samples CSV, in this order. The system DLLs are split by the nearest
// export name so waiting, locking and memory work do not hide behind one number.
enum Bucket { kGame, kCohtml, kV8, kRenoir, kD3D12, kDriver, kDxgi, kWait, kLock, kHeap, kMemcpy, kSystem, kDStorage, kAudio, kOther, kBucketCount };
const char* const kBucketNames[kBucketCount] = { "game", "cohtml", "v8", "renoir", "d3d12", "driver", "dxgi", "wait", "lock", "heap", "memcpy", "system", "dstorage", "audio", "other" };

struct ModuleRange { uintptr_t base; uintptr_t end; int bucket; bool system; char name[24]; };
ModuleRange g_modules[512];
int g_moduleCount = 0;

// labels a sample is tallied under: "module!export" for the system DLLs, the bucket name for
// everything else. An append only arena of strings, addressed by offset, sampler thread only.
std::vector<char> g_labels;
std::unordered_map<std::string, uint32_t> g_labelIndex;
uint32_t g_bucketLabel[kBucketCount];

uint32_t AddLabel(const char* text)
{
    auto known = g_labelIndex.find(text);
    if (known != g_labelIndex.end()) return known->second;
    uint32_t offset = (uint32_t)g_labels.size();
    g_labels.insert(g_labels.end(), text, text + strlen(text) + 1);
    g_labelIndex.emplace(text, offset);
    return offset;
}

// exports of the system DLLs, sorted by address, for the nearest export lookup
struct Export { uintptr_t addr; int bucket; uint32_t label; };
std::vector<Export> g_exports;

std::atomic<uint32_t> g_counts[kBucketCount];
std::atomic<uint32_t> g_unknownHits{0};

// every sample in a ring, and the frames as spans over it, so the minute's slowest frames can
// be told apart from its median frames after the fact. The caller of a sample outside the
// game packs the first two modules on the stack (indices into the module table plus one,
// 8 bits each) over the first game code address (64 byte bucket, 16 bits of the exe's 20).
struct Sample { uintptr_t rip; uint32_t label; uint32_t caller; };
inline uint32_t PackCaller(int via1, int via2, uint32_t gameKey) { return ((uint32_t)(via1 + 1) << 24) | ((uint32_t)(via2 + 1) << 16) | (gameKey & 0xFFFF); }
const char* ViaName(uint32_t caller, int which)
{
    int index = (int)((caller >> (which ? 16 : 24)) & 0xFF) - 1;
    return index < 0 ? "-" : g_modules[index].name;
}
const uint32_t kSampleRing = 1u << 19;
Sample g_samples[kSampleRing];
std::atomic<uint32_t> g_sampleWrite{0};

struct FrameSpan { float ms; uint32_t begin; uint32_t end; };
const uint32_t kFrameRing = 1u << 14;
FrameSpan g_spans[kFrameRing];
std::atomic<uint32_t> g_spanWrite{0};
uint32_t g_spanRead = 0;          // sampler thread
uint32_t g_nextSpanBegin = 0;     // render thread

uintptr_t g_gameBase = 0;
uintptr_t g_gameTextBegin = 0, g_gameTextEnd = 0;

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

void CollectExports(HMODULE mod, const char* file)
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
    char module[64];
    strncpy_s(module, file, _TRUNCATE);
    if (char* dot = strrchr(module, '.')) *dot = 0;
    char label[256];
    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        DWORD rva = functions[ordinals[i]];
        if (rva >= dir.VirtualAddress && rva < dir.VirtualAddress + dir.Size) continue;   // forwarder
        const char* name = (const char*)(base + names[i]);
        _snprintf_s(label, sizeof label, _TRUNCATE, "%s!%s", module, name);
        g_exports.push_back({ (uintptr_t)(base + rva), BucketForName(name), AddLabel(label) });
    }
}

void FindGameText(HMODULE mod)
{
    BYTE* base = (BYTE*)mod;
    auto dos = (IMAGE_DOS_HEADER*)base;
    auto nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    auto section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        if (memcmp(section->Name, ".text", 6) != 0) continue;
        g_gameTextBegin = (uintptr_t)base + section->VirtualAddress;
        g_gameTextEnd = g_gameTextBegin + section->Misc.VirtualSize;
        return;
    }
}

bool ReadQword(uintptr_t addr, uintptr_t* out)
{
    __try { *out = *(volatile uintptr_t*)addr; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

int ModuleIndexOf(uintptr_t addr)
{
    int lo = 0, hi = g_moduleCount;
    while (lo < hi) { int mid = (lo + hi) / 2; if (g_modules[mid].base <= addr) lo = mid + 1; else hi = mid; }
    if (lo == 0) return -1;
    return addr < g_modules[lo - 1].end ? lo - 1 : -1;
}

// What the suspended thread was called through: the first two modules outside the system
// DLLs whose code appears on top of the stack, and the first game code address, as a 64 byte
// bucket. Return addresses are what the sample was called from, give or take a function
// pointer that happens to sit on the stack. Packed as PackCaller, zero when nothing was found.
uint32_t CallerOnStack(uintptr_t rsp)
{
    if (!g_gameTextBegin) return 0;
    int via[2] = { -1, -1 };
    int found = 0;
    for (int i = 0; i < 768; ++i) {
        uintptr_t v;
        if (!ReadQword(rsp + i * 8, &v)) break;
        if (v >= g_gameTextBegin && v < g_gameTextEnd) return PackCaller(via[0], via[1], (uint32_t)((v - g_gameBase) >> 10));
        int m = ModuleIndexOf(v);
        if (m < 0 || g_modules[m].system || g_modules[m].bucket == kGame) continue;
        if (found == 0 || (found == 1 && m != via[0])) via[found++] = m;
    }
    return found ? PackCaller(via[0], via[1], 0) : 0;
}

void RefreshModules()
{
    HMODULE mods[512]; DWORD needed = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof mods, &needed)) return;
    int count = (int)std::min<DWORD>(needed / sizeof(HMODULE), 512);
    ModuleRange table[512]; int n = 0;
    g_exports.clear();
    for (int i = 0; i < count; ++i) {
        MODULEINFO info = {};
        char name[MAX_PATH] = {};
        if (!K32GetModuleInformation(GetCurrentProcess(), mods[i], &info, sizeof info)) continue;
        K32GetModuleFileNameExA(GetCurrentProcess(), mods[i], name, MAX_PATH);
        const char* file = strrchr(name, '\\');
        file = file ? file + 1 : name;
        bool system = false;
        int bucket = BucketForModule(file, &system);
        ModuleRange& range = table[n++];
        range = { (uintptr_t)info.lpBaseOfDll, (uintptr_t)info.lpBaseOfDll + info.SizeOfImage, bucket, system };
        strncpy_s(range.name, file, _TRUNCATE);
        if (char* dot = strrchr(range.name, '.')) *dot = 0;
        if (bucket == kGame) { g_gameBase = (uintptr_t)info.lpBaseOfDll; FindGameText(mods[i]); }
        if (system) CollectExports(mods[i], file);
    }
    std::sort(g_exports.begin(), g_exports.end(), [](const Export& a, const Export& b) { return a.addr < b.addr; });
    std::sort(table, table + n, [](const ModuleRange& a, const ModuleRange& b) { return a.base < b.base; });
    memcpy(g_modules, table, sizeof(ModuleRange) * n);
    g_moduleCount = n;
}

// bucket of the address, and the label to tally it under
int Classify(uintptr_t rip, uint32_t* label)
{
    *label = g_bucketLabel[kOther];
    int i = ModuleIndexOf(rip);
    if (i < 0) return -1;
    *label = g_bucketLabel[g_modules[i].bucket];
    if (!g_modules[i].system) return g_modules[i].bucket;
    // nearest export at or before the address
    size_t lo = 0, hi = g_exports.size();
    while (lo < hi) { size_t mid = (lo + hi) / 2; if (g_exports[mid].addr <= rip) lo = mid + 1; else hi = mid; }
    if (lo == 0) return kSystem;
    const Export& e = g_exports[lo - 1];
    if (e.addr < g_modules[i].base || rip - e.addr >= 0x4000) return kSystem;
    *label = e.label;
    return e.bucket;
}

// The minute's frames sorted by time: the slowest 1 percent against the median half, the
// same cut the report makes, so loading stalls and pauses do not colour the picture.
typedef std::unordered_map<uint64_t, uint32_t> Hist;

struct Ranked { uint64_t key; uint32_t slow; double fastScaled; };

std::vector<Ranked> Rank(const Hist& slow, const Hist& fast, double scale, size_t top, bool byExtra)
{
    std::vector<Ranked> rows;
    rows.reserve(slow.size() + fast.size());
    for (auto& kv : slow) {
        auto f = fast.find(kv.first);
        rows.push_back({ kv.first, kv.second, (f == fast.end() ? 0.0 : f->second) * scale });
    }
    if (!byExtra)
        for (auto& kv : fast)
            if (slow.find(kv.first) == slow.end()) rows.push_back({ kv.first, 0, kv.second * scale });
    std::sort(rows.begin(), rows.end(), [&](const Ranked& a, const Ranked& b) {
        return byExtra ? (a.slow - a.fastScaled) > (b.slow - b.fastScaled) : a.fastScaled > b.fastScaled;
    });
    if (rows.size() > top) rows.resize(top);
    return rows;
}

void LogHotSpots()
{
    uint32_t write = g_spanWrite.load();
    std::vector<FrameSpan> frames;
    for (uint32_t i = g_spanRead; i != write; ++i) {
        const FrameSpan& s = g_spans[i % kFrameRing];
        if (s.ms < 100.0f && s.end != s.begin) frames.push_back(s);
    }
    g_spanRead = write;
    if (frames.size() < 200) return;

    std::sort(frames.begin(), frames.end(), [](const FrameSpan& a, const FrameSpan& b) { return a.ms < b.ms; });
    size_t slowCount = std::max<size_t>(5, frames.size() / 100);
    size_t fastCount = frames.size() / 2;
    Hist gameSlow, gameFast, outSlow, outFast, pairSlow, pairFast;
    uint32_t slowSamples = 0, fastSamples = 0;
    double slowMs = 0.0, fastMs = 0.0;
    auto tally = [&](const FrameSpan& f, Hist& game, Hist& out, Hist& pair, uint32_t& samples) {
        for (uint32_t i = f.begin; i != f.end; ++i) {
            const Sample& s = g_samples[i % kSampleRing];
            samples++;
            if (s.rip >= g_gameTextBegin && s.rip < g_gameTextEnd) { game[(s.rip - g_gameBase) >> 6]++; continue; }
            out[s.label]++;
            pair[((uint64_t)s.label << 32) | s.caller]++;
        }
    };
    for (size_t i = frames.size() - slowCount; i < frames.size(); ++i) { tally(frames[i], gameSlow, outSlow, pairSlow, slowSamples); slowMs += frames[i].ms; }
    for (size_t i = 0; i < fastCount; ++i) { tally(frames[i], gameFast, outFast, pairFast, fastSamples); fastMs += frames[i].ms; }
    if (!slowSamples || !fastSamples) return;
    double scale = (double)slowSamples / fastSamples;

    Log("sampler: last minute, %zu frames: slowest 1%% = %zu frames over %.1f ms (mean %.1f ms, %.1f samples each), median half under %.1f ms (mean %.1f ms, %.1f samples each)",
        frames.size(), slowCount, frames[frames.size() - slowCount].ms, slowMs / slowCount, (double)slowSamples / slowCount,
        frames[fastCount].ms, fastMs / fastCount, (double)fastSamples / fastCount);
    Log("sampler: game code with the most extra samples in the slowest 1%% (rva, slow, median half scaled)");
    for (const Ranked& r : Rank(gameSlow, gameFast, scale, 12, true))
        Log("sampler:   rva 0x%06X  slow %5u  fast %7.1f", (unsigned)(r.key << 6), r.slow, r.fastScaled);
    Log("sampler: outside the game code, functions with the most extra samples in the slowest 1%%");
    for (const Ranked& r : Rank(outSlow, outFast, scale, 10, true))
        Log("sampler:   %-44s slow %5u  fast %7.1f", g_labels.data() + r.key, r.slow, r.fastScaled);
    Log("sampler: outside the game code, functions with the most samples in the median half (the steady cost)");
    for (const Ranked& r : Rank(outSlow, outFast, scale, 8, false))
        Log("sampler:   %-44s slow %5u  fast %7.1f", g_labels.data() + r.key, r.slow, r.fastScaled);
    Log("sampler: function, the modules it was called through and the game call site under it, most extra samples in the slowest 1%% first");
    for (const Ranked& r : Rank(pairSlow, pairFast, scale, 14, true)) {
        uint32_t caller = (uint32_t)(r.key & 0xFFFFFFFF);
        Log("sampler:   %-40s via %s > %s  rva 0x%06X (1 KB)  slow %5u  fast %7.1f", g_labels.data() + (uint32_t)(r.key >> 32),
            ViaName(caller, 0), ViaName(caller, 1), (unsigned)((caller & 0xFFFF) << 10), r.slow, r.fastScaled);
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
    g_labels.reserve(1 << 20);
    for (int b = 0; b < kBucketCount; ++b) g_bucketLabel[b] = AddLabel(kBucketNames[b]);
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
                uintptr_t rip = (uintptr_t)ctx.Rip;
                uint32_t label = g_bucketLabel[kOther];
                int bucket = ok ? Classify(rip, &label) : -1;
                // the stack is read while the thread is still suspended
                uint32_t caller = (ok && bucket != kGame) ? CallerOnStack((uintptr_t)ctx.Rsp) : 0;
                ResumeThread(thread);
                if (ok) {
                    if (bucket < 0) { g_unknownHits++; bucket = kOther; }
                    g_counts[bucket]++;
                    uint32_t w = g_sampleWrite.load();
                    g_samples[w % kSampleRing] = { rip, label, caller };
                    g_sampleWrite.store(w + 1);
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

    // the frame that just ended owns the samples taken since the previous present
    uint32_t end = g_sampleWrite.load();
    uint32_t w = g_spanWrite.load();
    g_spans[w % kFrameRing] = { (float)frameMs, g_nextSpanBegin, end };
    g_spanWrite.store(w + 1);
    g_nextSpanBegin = end;
}

void StartSampler()
{
    if (!g_cfg.sampler) return;
    static HANDLE thread = nullptr;
    if (thread) return;
    InitializeCriticalSection(&g_recordCs);
    thread = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr);
    Log("sampler: started, one sample every %d us, buckets: game, cohtml, v8, renoir, d3d12, driver, dxgi, wait, lock, heap, memcpy, system, dstorage, audio, other", g_cfg.sampleUs);
}
