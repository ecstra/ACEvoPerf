// Load sampler, see include/acevo/telemetry/load_sampler.h.
//
// Every refresh the busiest threads of the process are picked by their CPU time,
// which during a load is the engine's resource workers without having to know
// their names. They are then sampled round robin: suspend, read RIP and RSP,
// resume, and only then classify, because allocating while another thread is
// suspended is how a profiler deadlocks on the heap lock.
//
// The classification is the one the render thread sampler used (commit a119e8c):
// the module the address belongs to, and for the system DLLs the nearest export,
// so waiting, locking, heap and memcpy do not hide behind one "ntdll".
#include "acevo/telemetry/load_sampler.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/engine/job_lock.h"
#include <tlhelp32.h>
#include <unordered_map>

namespace {

enum Bucket { kGame, kCohtml, kV8, kRenoir, kD3D12, kDriver, kDxgi, kWait, kLock, kHeap, kMemcpy, kSystem, kDStorage, kAudio, kOther, kBucketCount };
const char* const kBucketNames[kBucketCount] = { "game", "cohtml", "v8", "renoir", "d3d12", "driver", "dxgi", "wait", "lock", "heap", "memcpy", "system", "dstorage", "audio", "other" };

struct ModuleRange { uintptr_t base, end; int bucket; bool system; char name[24]; };
ModuleRange g_modules[512];
int g_moduleCount = 0;
uintptr_t g_gameBase = 0, g_gameTextBegin = 0, g_gameTextEnd = 0;

std::vector<char> g_labels;
std::unordered_map<std::string, uint32_t> g_labelIndex;
uint32_t g_bucketLabel[kBucketCount];

struct Export { uintptr_t addr; int bucket; uint32_t label; };
std::vector<Export> g_exports;

// A thread worth sampling, refreshed by CPU time.
struct Target {
    DWORD tid = 0;
    HANDLE handle = nullptr;
    uint64_t lastCpu = 0;
    char name[40] = {};
};
Target g_targets[24];
int g_targetCount = 0;

// tallies
std::unordered_map<uint32_t, uint32_t> g_byLabel;    // label offset -> samples
std::unordered_map<uint32_t, uint32_t> g_byGameRva;  // 64 byte bucket of the exe rva -> samples
// Per thread, the buckets it was sampled in. Threads are sampled round robin, so
// within one thread the shares are an unbiased read of how it spent its time, which
// is the only way to tell a worker that is grinding from one that is parked.
struct ThreadTally { uint32_t total = 0; uint32_t bucket[kBucketCount] = {}; uint32_t spin = 0; };
std::unordered_map<std::string, ThreadTally> g_byThread;
// the game's fiber job queue spin loop, found on 2026-09-12, see TODO-013
const uint32_t kJobSpinRvaLo = 0x279fa90 >> 6, kJobSpinRvaHi = 0x279fad4 >> 6;
// Set when job_lock_fix patched the loop, because the hot address then lives in the mod's page.
uintptr_t g_spinCaveLo = 0;
uintptr_t g_spinCaveHi = 0;
uint64_t g_bucketCounts[kBucketCount] = {};
uint64_t g_total = 0, g_failed = 0, g_grandTotal = 0;

HANDLE g_thread = nullptr;
HANDLE g_csv = INVALID_HANDLE_VALUE;
volatile LONG g_stop = 0;
volatile LONG g_exited = 0;

typedef HRESULT (WINAPI *PFN_GetThreadDescription)(HANDLE, PWSTR*);
PFN_GetThreadDescription g_getThreadDescription = nullptr;

uint32_t AddLabel(const char* text)
{
    auto known = g_labelIndex.find(text);
    if (known != g_labelIndex.end()) return known->second;
    uint32_t offset = (uint32_t)g_labels.size();
    g_labels.insert(g_labels.end(), text, text + strlen(text) + 1);
    g_labelIndex.emplace(text, offset);
    return offset;
}

int BucketForName(const char* name)
{
    // The lock primitives go first. NtWaitForAlertByThreadId is what an SRWLock, a
    // critical section and a condition variable all block in, and it matches the
    // generic "WaitFor" test below, so checking that first would file every lock in
    // the process under waiting and make the split useless.
    if (strstr(name, "CriticalSection") || strstr(name, "SRWLock") || strstr(name, "NtWaitForAlertByThreadId") || strstr(name, "RtlpWaitOn")
        || strstr(name, "AcquireSRW") || strstr(name, "ReleaseSRW") || strstr(name, "RtlWaitOnAddress")
        || strstr(name, "NtAlertThreadByThreadId")) return kLock;
    if (strstr(name, "NtWaitFor") || strstr(name, "NtDelayExecution") || strstr(name, "NtRemoveIoCompletion")
        || strstr(name, "WaitFor") || strstr(name, "SleepConditionVariable") || strstr(name, "SleepEx") || strstr(name, "NtSignalAndWait")
        || strstr(name, "ZwWaitForWorkViaWorkerFactory") || strstr(name, "NtYieldExecution")) return kWait;
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
        if (rva >= dir.VirtualAddress && rva < dir.VirtualAddress + dir.Size) continue;
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
        range = { (uintptr_t)info.lpBaseOfDll, (uintptr_t)info.lpBaseOfDll + info.SizeOfImage, bucket, system, {} };
        strncpy_s(range.name, file, _TRUNCATE);
        if (char* dot = strrchr(range.name, '.')) *dot = 0;
        if (bucket == kGame) { g_gameBase = (uintptr_t)info.lpBaseOfDll; FindGameText(mods[i]); }
        if (system) CollectExports(mods[i], file);
        if (n >= 512) break;
    }
    std::sort(g_exports.begin(), g_exports.end(), [](const Export& a, const Export& b) { return a.addr < b.addr; });
    std::sort(table, table + n, [](const ModuleRange& a, const ModuleRange& b) { return a.base < b.base; });
    memcpy(g_modules, table, sizeof(ModuleRange) * n);
    g_moduleCount = n;
}

int ModuleIndexOf(uintptr_t addr)
{
    int lo = 0, hi = g_moduleCount;
    while (lo < hi) { int mid = (lo + hi) / 2; if (g_modules[mid].base <= addr) lo = mid + 1; else hi = mid; }
    if (lo == 0) return -1;
    return addr < g_modules[lo - 1].end ? lo - 1 : -1;
}

int Classify(uintptr_t rip, uint32_t* label)
{
    *label = g_bucketLabel[kOther];
    int i = ModuleIndexOf(rip);
    if (i < 0) return -1;
    *label = g_bucketLabel[g_modules[i].bucket];
    if (!g_modules[i].system) return g_modules[i].bucket;
    size_t lo = 0, hi = g_exports.size();
    while (lo < hi) { size_t mid = (lo + hi) / 2; if (g_exports[mid].addr <= rip) lo = mid + 1; else hi = mid; }
    if (lo == 0) return kSystem;
    const Export& e = g_exports[lo - 1];
    if (e.addr < g_modules[i].base || rip - e.addr >= 0x4000) return kSystem;
    *label = e.label;
    return e.bucket;
}

uint64_t ThreadCpu(HANDLE h)
{
    FILETIME c, e, k, u;
    if (!GetThreadTimes(h, &c, &e, &k, &u)) return 0;
    return (((uint64_t)k.dwHighDateTime << 32) | k.dwLowDateTime) + (((uint64_t)u.dwHighDateTime << 32) | u.dwLowDateTime);
}

void NameThread(HANDLE h, DWORD tid, char* out, size_t n)
{
    out[0] = 0;
    if (g_getThreadDescription) {
        PWSTR desc = nullptr;
        if (SUCCEEDED(g_getThreadDescription(h, &desc)) && desc) {
            if (desc[0]) WideCharToMultiByte(CP_UTF8, 0, desc, -1, out, (int)n, nullptr, nullptr);
            LocalFree(desc);
        }
    }
    if (!out[0]) _snprintf_s(out, n, _TRUNCATE, "tid %lu", tid);
}

// Pick the threads that burned the most CPU since the last refresh. During a load those
// are the resource workers, without needing to know what the engine calls them.
void RefreshTargets()
{
    struct Candidate { DWORD tid; HANDLE h; uint64_t cpu, delta; };
    std::vector<Candidate> found;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te = {}; te.dwSize = sizeof te;
    DWORD me = GetCurrentProcessId(), self = GetCurrentThreadId();
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != me || te.th32ThreadID == self) continue;
            HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
            if (!h) continue;
            uint64_t cpu = ThreadCpu(h);
            uint64_t prev = 0;
            for (int i = 0; i < g_targetCount; ++i) if (g_targets[i].tid == te.th32ThreadID) { prev = g_targets[i].lastCpu; break; }
            found.push_back({ te.th32ThreadID, h, cpu, cpu > prev ? cpu - prev : 0 });
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);

    std::sort(found.begin(), found.end(), [](const Candidate& a, const Candidate& b) { return a.delta > b.delta; });
    for (int i = 0; i < g_targetCount; ++i) if (g_targets[i].handle) CloseHandle(g_targets[i].handle);
    g_targetCount = 0;
    for (auto& c : found) {
        if (g_targetCount < (int)(sizeof g_targets / sizeof g_targets[0]) && (c.delta > 0 || g_targetCount < 4)) {
            Target& t = g_targets[g_targetCount++];
            t.tid = c.tid; t.handle = c.h; t.lastCpu = c.cpu;
            NameThread(c.h, c.tid, t.name, sizeof t.name);
        } else {
            CloseHandle(c.h);
        }
    }
}

void WriteCsvLine(double seconds)
{
    if (g_csv == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME st; GetLocalTime(&st);
    char line[512];
    int n = _snprintf_s(line, sizeof line, _TRUNCATE, "%02d:%02d:%02d,%.1f,%llu", st.wHour, st.wMinute, st.wSecond, seconds, (unsigned long long)g_total);
    for (int b = 0; b < kBucketCount; ++b) {
        int m = _snprintf_s(line + n, sizeof line - n, _TRUNCATE, ",%llu", (unsigned long long)g_bucketCounts[b]);
        n += m;
    }
    n += _snprintf_s(line + n, sizeof line - n, _TRUNCATE, "\r\n");
    DWORD written; WriteFile(g_csv, line, (DWORD)n, &written, nullptr);
}

template <typename Map>
std::vector<std::pair<uint32_t, uint32_t>> Top(const Map& m, size_t count)
{
    std::vector<std::pair<uint32_t, uint32_t>> rows(m.begin(), m.end());
    std::sort(rows.begin(), rows.end(), [](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b) { return a.second > b.second; });
    if (rows.size() > count) rows.resize(count);
    return rows;
}

// Each summary covers only the window since the previous one, so a load does not get
// averaged together with the minutes of menu idling on either side of it.
void LogSummary(const char* when)
{
    if (!g_total) { Log("[loadsampler] %s: nothing sampled", when); return; }
    Log("[loadsampler] %s: %llu samples (%llu unreadable). Where the busiest threads were:", when,
        (unsigned long long)g_total, (unsigned long long)g_failed);
    for (int b = 0; b < kBucketCount; ++b) {
        if (!g_bucketCounts[b]) continue;
        Log("[loadsampler]   %-9s %7llu  %5.1f%%", kBucketNames[b], (unsigned long long)g_bucketCounts[b], 100.0 * g_bucketCounts[b] / g_total);
    }
    Log("[loadsampler] the functions outside the game code, most samples first");
    for (auto& r : Top(g_byLabel, 16))
        Log("[loadsampler]   %-46s %7u  %5.1f%%", g_labels.data() + r.first, r.second, 100.0 * r.second / g_total);
    Log("[loadsampler] game code, 64 byte buckets of the exe rva, most samples first");
    for (auto& r : Top(g_byGameRva, 16))
        Log("[loadsampler]   rva 0x%08X  %7u  %5.1f%%", (unsigned)((uint64_t)r.first << 6), r.second, 100.0 * r.second / g_total);
    // Per thread the shares are of that thread's own samples, so they say how it spent
    // its time. "jobspin" is the share inside the engine's fiber job queue spin loop.
    Log("[loadsampler] per thread, shares of that thread's own samples: game / jobspin / lock / wait / other");
    std::vector<std::pair<std::string, ThreadTally>> threads(g_byThread.begin(), g_byThread.end());
    std::sort(threads.begin(), threads.end(), [](const std::pair<std::string, ThreadTally>& a, const std::pair<std::string, ThreadTally>& b) {
        return (a.second.total - a.second.bucket[kWait]) > (b.second.total - b.second.bucket[kWait]);
    });
    for (size_t i = 0; i < threads.size() && i < 16; ++i) {
        const ThreadTally& v = threads[i].second;
        if (!v.total) continue;
        double n = v.total;
        uint32_t rest = v.total - v.bucket[kGame] - v.bucket[kLock] - v.bucket[kWait];
        Log("[loadsampler]   %-28s %5u samples  game %5.1f%%  jobspin %5.1f%%  lock %5.1f%%  wait %5.1f%%  other %5.1f%%",
            threads[i].first.c_str(), v.total, 100.0 * v.bucket[kGame] / n, 100.0 * v.spin / n,
            100.0 * v.bucket[kLock] / n, 100.0 * v.bucket[kWait] / n, 100.0 * rest / n);
    }

    g_byLabel.clear();
    g_byGameRva.clear();
    g_byThread.clear();
    memset(g_bucketCounts, 0, sizeof g_bucketCounts);
    g_grandTotal += g_total;
    g_total = 0;
    g_failed = 0;
}

DWORD WINAPI SamplerThread(void*)
{
    SetThreadDescription(GetCurrentThread(), L"ACEvoPerf load sampler");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    g_labels.reserve(1 << 20);
    for (int b = 0; b < kBucketCount; ++b) g_bucketLabel[b] = AddLabel(kBucketNames[b]);
    RefreshModules();
    RefreshTargets();
    Log("[loadsampler] %d modules, %zu system exports, %d thread(s) in the first round, one sample every %d us",
        g_moduleCount, g_exports.size(), g_targetCount, g_cfg.loadSampleUs);

    std::wstring path = g_dir + L"acevo_perf_load_samples.csv";
    g_csv = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_csv != INVALID_HANDLE_VALUE) {
        std::string header = "clock,t_s,samples";
        for (int b = 0; b < kBucketCount; ++b) { header += ","; header += kBucketNames[b]; }
        header += "\r\n";
        DWORD written; WriteFile(g_csv, header.data(), (DWORD)header.size(), &written, nullptr);
    }

    LARGE_INTEGER qpf; QueryPerformanceFrequency(&qpf);
    LARGE_INTEGER start; QueryPerformanceCounter(&start);
    const int64_t interval = qpf.QuadPart * (int64_t)g_cfg.loadSampleUs / 1000000;
    LARGE_INTEGER next = start;
    int64_t lastCsv = start.QuadPart, lastRefresh = start.QuadPart, lastSummary = start.QuadPart;
    int cursor = 0, refreshes = 0;
    CONTEXT ctx;

    while (!g_stop) {
        next.QuadPart += interval;
        LARGE_INTEGER now;
        // Sleep the wait away rather than spinning it. A spin here would hold a whole
        // core at the highest priority and take it from the very workers being measured.
        for (;;) {
            QueryPerformanceCounter(&now);
            if (now.QuadPart >= next.QuadPart || g_stop) break;
            int64_t leftUs = (next.QuadPart - now.QuadPart) * 1000000 / qpf.QuadPart;
            if (leftUs > 1500) Sleep((DWORD)(leftUs / 1000) - 1);
            else YieldProcessor();
        }
        if (g_stop) break;
        if (next.QuadPart < now.QuadPart - interval * 8) next.QuadPart = now.QuadPart;

        if (g_targetCount) {
            Target& t = g_targets[cursor % g_targetCount];
            cursor++;
            uintptr_t rip = 0;
            bool ok = false;
            // Nothing between Suspend and Resume may allocate: the suspended thread
            // can be holding the heap lock.
            if (SuspendThread(t.handle) != (DWORD)-1) {
                ctx.ContextFlags = CONTEXT_CONTROL;
                if (GetThreadContext(t.handle, &ctx)) { rip = (uintptr_t)ctx.Rip; ok = true; }
                ResumeThread(t.handle);
            }
            if (!ok) {
                g_failed++;
            } else {
                uint32_t label = 0;
                int bucket = Classify(rip, &label);
                if (bucket < 0) bucket = kOther;
                g_total++;
                g_bucketCounts[bucket]++;
                ThreadTally& tally = g_byThread[t.name];
                tally.total++;
                tally.bucket[bucket]++;
                if (rip >= g_gameTextBegin && rip < g_gameTextEnd) {
                    uint32_t key = (uint32_t)((rip - g_gameBase) >> 6);
                    g_byGameRva[key]++;
                    if (key >= kJobSpinRvaLo && key <= kJobSpinRvaHi) tally.spin++;
                } else {
                    // With [engine] job_lock_fix=1 the spin runs from the mod's own page instead of
                    // the exe, so it has to be counted here too. Otherwise a spin that merely moved
                    // would read as a spin that stopped.
                    if (g_spinCaveLo && rip >= g_spinCaveLo && rip < g_spinCaveHi) tally.spin++;
                    g_byLabel[label]++;
                }
            }
        }

        if (now.QuadPart - lastCsv > qpf.QuadPart) {
            WriteCsvLine((double)(now.QuadPart - start.QuadPart) / qpf.QuadPart);
            lastCsv = now.QuadPart;
        }
        if (now.QuadPart - lastRefresh > qpf.QuadPart * 2) {
            // Modules keep arriving well after the game is up, and an address in one the
            // table does not know yet would be tallied as "other".
            if (++refreshes % 5 == 0) RefreshModules();
            RefreshTargets();
            lastRefresh = now.QuadPart;
        }
        if (now.QuadPart - lastSummary > qpf.QuadPart * 15) {
            char when[64];
            _snprintf_s(when, sizeof when, _TRUNCATE, "t+%.0f s, the last %.0f s",
                (double)(now.QuadPart - start.QuadPart) / qpf.QuadPart, (double)(now.QuadPart - lastSummary) / qpf.QuadPart);
            LogSummary(when);
            lastSummary = now.QuadPart;
        }
    }
    InterlockedExchange(&g_exited, 1);
    return 0;
}

} // namespace

void StartLoadSampler()
{
    if (!g_cfg.loadSampler || g_thread) return;
    const BYTE* caveLo = nullptr;
    const BYTE* caveHi = nullptr;
    JobLockCave(&caveLo, &caveHi);
    g_spinCaveLo = (uintptr_t)caveLo;
    g_spinCaveHi = (uintptr_t)caveHi;
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (k32) g_getThreadDescription = (PFN_GetThreadDescription)GetProcAddress(k32, "GetThreadDescription");
    g_thread = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr);
    Log("[loadsampler] started. Drive one session load, then quit, and read acevo_perf_load_samples.csv next to the exe.");
}

void StopLoadSampler()
{
    if (!g_thread) return;
    InterlockedExchange(&g_stop, 1);
    // This runs from DllMain on detach, under the loader lock, so the sampler thread is
    // never joined: a join there is how a DLL hangs a process on the way out. Poll its
    // own flag for a moment instead and skip the last summary if it does not answer.
    for (int i = 0; i < 40 && !g_exited; ++i) Sleep(5);
    if (g_exited) LogSummary("final window");
    Log("[loadsampler] %llu samples in total", (unsigned long long)(g_grandTotal + g_total));
    if (g_csv != INVALID_HANDLE_VALUE) CloseHandle(g_csv);
    g_csv = INVALID_HANDLE_VALUE;
    CloseHandle(g_thread);
    g_thread = nullptr;
}
