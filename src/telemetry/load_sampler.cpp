// Load sampler, see include/acevo/telemetry/load_sampler.h.
//
// Every refresh the busiest threads of the process are picked by their CPU time,
// which during a load is the engine's resource workers without having to know
// their names. They are then sampled round robin: suspend, read RIP, resume,
// and only then classify, because allocating while another thread is
// suspended is how a profiler deadlocks on the heap lock.
//
// The classification is the one the render thread sampler used (commit a119e8c):
// the module the address belongs to, and for the system DLLs the nearest export,
// so waiting, locking, heap and memcpy do not hide behind one "ntdll".
#include "acevo/telemetry/load_sampler.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/render/frame_stats.h"
#include <tlhelp32.h>
#include <map>
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
    uint64_t created = 0;
    HANDLE handle = nullptr;
    char name[40] = {};
};
Target g_targets[24];
int g_targetCount = 0;
// Every thread's CPU time at the last refresh, targets or not, so each one's delta is its own. The
// creation time tells a new thread from an exited one whose id it was given.
struct Seen { uint64_t created, cpu; };
std::unordered_map<DWORD, Seen> g_lastSeen;

// tallies
std::unordered_map<uint32_t, uint32_t> g_byLabel;    // label offset -> samples
std::unordered_map<uint32_t, uint32_t> g_byGameRva;  // 64 byte bucket of the exe rva -> samples
// Per thread, the buckets it was sampled in. Threads are sampled round robin, so
// within one thread the shares are an unbiased read of how it spent its time, which
// is the only way to tell a worker that is grinding from one that is parked. Keyed by
// the thread's id and creation time with the last name it was sampled under. The game
// names its main thread after the sampler has met it, so keyed by name that one thread
// made a "tid N" row and a GameThread row, and an exited thread's id can go to a new
// thread inside one window, so the id alone would put two threads in one row.
struct ThreadTally { uint32_t total = 0; uint32_t bucket[kBucketCount] = {}; uint32_t spin = 0; char name[40] = {}; };
using ThreadKey = std::pair<DWORD, uint64_t>;
std::map<ThreadKey, ThreadTally> g_byThread;
// the game's fiber job queue spin loop, found on 2026-09-12, see TODO-013
const uint32_t kJobSpinRvaLo = 0x279fa90 >> 6, kJobSpinRvaHi = 0x279fad4 >> 6;
uint64_t g_bucketCounts[kBucketCount] = {};
uint64_t g_total = 0, g_failed = 0, g_grandTotal = 0;
// The CSV's own counts, zeroed at each line so a row is its own second. The summary's are zeroed every
// fifteen seconds, and written raw they sawtoothed with nothing in the file to say so.
uint64_t g_csvCounts[kBucketCount] = {}, g_csvTotal = 0;
uint64_t g_samplerCpu = 0;  // the sampler thread's own CPU time at the last summary

HANDLE g_thread = nullptr;
HANDLE g_csv = INVALID_HANDLE_VALUE;
volatile LONG g_stop = 0;

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
        || strstr(name, "NtYieldExecution")) return kWait;
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
    char label[256], twin[128];
    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        DWORD rva = functions[ordinals[i]];
        if (rva >= dir.VirtualAddress && rva < dir.VirtualAddress + dir.Size) continue;
        const char* name = (const char*)(base + names[i]);
        // ntdll exports every system call twice, NtX and ZwX at one address, and the sort by address
        // leaves to chance which twin a sample finds. The buckets know the Nt names, so a Zw name is
        // read as its Nt twin and neither the bucket nor the label depends on the sort.
        if (name[0] == 'Z' && name[1] == 'w') {
            _snprintf_s(twin, sizeof twin, _TRUNCATE, "Nt%s", name + 2);
            name = twin;
        }
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

uint64_t ThreadCpu(HANDLE h, uint64_t* created = nullptr)
{
    FILETIME c, e, k, u;
    if (!GetThreadTimes(h, &c, &e, &k, &u)) return 0;
    if (created) *created = ((uint64_t)c.dwHighDateTime << 32) | c.dwLowDateTime;
    return (((uint64_t)k.dwHighDateTime << 32) | k.dwLowDateTime) + (((uint64_t)u.dwHighDateTime << 32) | u.dwLowDateTime);
}

void NameThread(HANDLE h, DWORD tid, char* out, size_t n)
{
    out[0] = 0;
    if (g_getThreadDescription) {
        PWSTR desc = nullptr;
        if (SUCCEEDED(g_getThreadDescription(h, &desc)) && desc) {
            // Converted whole and then cut, since a name longer than the buffer fails the conversion
            // with the buffer full and no terminator, and the table would print on past it.
            char utf8[256];
            if (desc[0] && WideCharToMultiByte(CP_UTF8, 0, desc, -1, utf8, sizeof utf8, nullptr, nullptr))
                strncpy_s(out, n, utf8, _TRUNCATE);
            LocalFree(desc);
        }
    }
    if (!out[0]) _snprintf_s(out, n, _TRUNCATE, "tid %lu", tid);
}

// Pick the threads that burned the most CPU since the last refresh. During a load those
// are the resource workers, without needing to know what the engine calls them.
void RefreshTargets()
{
    struct Candidate { DWORD tid; HANDLE h; uint64_t created, cpu, delta; };
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
            uint64_t created = 0;
            uint64_t cpu = ThreadCpu(h, &created);
            // Measured from the last refresh for every thread. Only the targets used to be, so every other
            // thread showed its whole lifetime as its delta, and the slots the busiest threads did not hold
            // went back and forth between long lived threads, busy or parked. A thread new since then, even
            // one given the id of a thread that exited, has lived only inside the window, so its whole time
            // is its delta.
            auto last = g_lastSeen.find(te.th32ThreadID);
            uint64_t prev = last != g_lastSeen.end() && last->second.created == created ? last->second.cpu : 0;
            found.push_back({ te.th32ThreadID, h, created, cpu, cpu > prev ? cpu - prev : 0 });
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);

    g_lastSeen.clear();
    for (auto& c : found) g_lastSeen[c.tid] = { c.created, c.cpu };

    std::sort(found.begin(), found.end(), [](const Candidate& a, const Candidate& b) { return a.delta > b.delta; });
    for (int i = 0; i < g_targetCount; ++i) if (g_targets[i].handle) CloseHandle(g_targets[i].handle);
    g_targetCount = 0;
    for (auto& c : found) {
        if (g_targetCount < (int)(sizeof g_targets / sizeof g_targets[0]) && (c.delta > 0 || g_targetCount < 4)) {
            Target& t = g_targets[g_targetCount++];
            t.tid = c.tid; t.created = c.created; t.handle = c.h;
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
    int n = _snprintf_s(line, sizeof line, _TRUNCATE, "%02d:%02d:%02d,%.1f,%llu", st.wHour, st.wMinute, st.wSecond, seconds, (unsigned long long)g_csvTotal);
    for (int b = 0; b < kBucketCount; ++b) {
        int m = _snprintf_s(line + n, sizeof line - n, _TRUNCATE, ",%llu", (unsigned long long)g_csvCounts[b]);
        n += m;
    }
    n += _snprintf_s(line + n, sizeof line - n, _TRUNCATE, "\r\n");
    DWORD written; WriteFile(g_csv, line, (DWORD)n, &written, nullptr);
    g_csvTotal = 0;
    memset(g_csvCounts, 0, sizeof g_csvCounts);
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
    // What the sampler cost in the window sits beside what it measured.
    uint64_t samplerCpu = ThreadCpu(GetCurrentThread());
    double samplerMs = (samplerCpu - g_samplerCpu) / 10000.0;
    g_samplerCpu = samplerCpu;

    if (!g_total) { Log("[loadsampler] %s: nothing sampled, the sampler used %.0f ms of CPU", when, samplerMs); return; }
    Log("[loadsampler] %s: %llu samples (%llu unreadable), the sampler used %.0f ms of CPU. Where the busiest threads were:", when,
        (unsigned long long)g_total, (unsigned long long)g_failed, samplerMs);
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
    // Ranked by the samples a thread was running, outside wait and lock. A thread parked on a
    // condition variable or a lock sits in NtWaitForAlertByThreadId, which files as lock, so
    // ranked by everything outside wait it took the rows of the workers busy in a short load.
    Log("[loadsampler] per thread, shares of that thread's own samples: game / jobspin / lock / wait / other");
    std::vector<std::pair<ThreadKey, ThreadTally>> threads(g_byThread.begin(), g_byThread.end());
    auto running = [](const ThreadTally& t) { return t.total - t.bucket[kWait] - t.bucket[kLock]; };
    std::sort(threads.begin(), threads.end(), [&](const std::pair<ThreadKey, ThreadTally>& a, const std::pair<ThreadKey, ThreadTally>& b) {
        return running(a.second) > running(b.second);
    });
    for (size_t i = 0; i < threads.size() && i < 16; ++i) {
        const ThreadTally& v = threads[i].second;
        if (!v.total) continue;
        double n = v.total;
        uint32_t rest = v.total - v.bucket[kGame] - v.bucket[kLock] - v.bucket[kWait];
        // The id goes beside the name because the game runs threads that share one. It rebuilds its
        // workers under the same names each time the loading boost goes on or off, and runs a second
        // Resource Manager Worker 0 during a load.
        Log("[loadsampler]   %-28s tid %-6lu %5u samples  game %5.1f%%  jobspin %5.1f%%  lock %5.1f%%  wait %5.1f%%  other %5.1f%%",
            v.name, (unsigned long)threads[i].first.first, v.total, 100.0 * v.bucket[kGame] / n, 100.0 * v.spin / n,
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
    // Said, because a copy left on disk from an earlier run would otherwise be read as this one's.
    if (g_csv == INVALID_HANDLE_VALUE)
        Log("[loadsampler] could not create acevo_perf_load_samples.csv (error %lu), the summaries still come to this log", GetLastError());
    if (g_csv != INVALID_HANDLE_VALUE) {
        std::string header = "clock,t_s,samples";
        for (int b = 0; b < kBucketCount; ++b) { header += ","; header += kBucketNames[b]; }
        header += "\r\n";
        DWORD written; WriteFile(g_csv, header.data(), (DWORD)header.size(), &written, nullptr);
    }

    LARGE_INTEGER qpf; QueryPerformanceFrequency(&qpf);
    LARGE_INTEGER start; QueryPerformanceCounter(&start);
    g_samplerCpu = ThreadCpu(GetCurrentThread());  // so the first window's cost leaves out the setup above
    const int64_t interval = qpf.QuadPart * (int64_t)g_cfg.loadSampleUs / 1000000;
    LARGE_INTEGER next = start;
    int64_t lastCsv = start.QuadPart, lastRefresh = start.QuadPart, lastSummary = start.QuadPart;
    int cursor = 0, refreshes = 0;
    CONTEXT ctx;
    // The wait blocks on a high resolution timer. Sleep counts only whole milliseconds, and SwitchToThread
    // returns at once when nothing else is queued on the processor, so the loop they made spun a whole
    // core at the highest priority for the session, taken from the very workers being measured.
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

    while (!g_stop) {
        next.QuadPart += interval;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        LARGE_INTEGER due;  // negative is relative, in 100 ns units
        due.QuadPart = -((next.QuadPart - now.QuadPart) * 10000000 / qpf.QuadPart);
        if (due.QuadPart < 0) {
            // Without the wait the loop would suspend game threads as fast as it can.
            if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) || WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0) {
                Log("[loadsampler] its timer failed (error %lu), sampling stops here", GetLastError());
                break;
            }
            QueryPerformanceCounter(&now);
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
                g_csvTotal++;
                g_csvCounts[bucket]++;
                ThreadTally& tally = g_byThread[{ t.tid, t.created }];
                memcpy(tally.name, t.name, sizeof tally.name);
                tally.total++;
                tally.bucket[bucket]++;
                if (rip >= g_gameTextBegin && rip < g_gameTextEnd) {
                    uint32_t key = (uint32_t)((rip - g_gameBase) >> 6);
                    g_byGameRva[key]++;
                    if (key >= kJobSpinRvaLo && key <= kJobSpinRvaHi) tally.spin++;
                } else {
                    g_byLabel[label]++;
                }
            }
        }

        if (now.QuadPart - lastCsv > qpf.QuadPart) {
            // Seconds since attach, the clock the timeline, frames and trace files count in, so a
            // row lines up with theirs. The sampler itself starts seconds later.
            WriteCsvLine(NowSec());
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
                NowSec(), (double)(now.QuadPart - lastSummary) / qpf.QuadPart);
            LogSummary(when);
            lastSummary = now.QuadPart;
        }
    }
    if (timer) CloseHandle(timer);
    return 0;
}

} // namespace

void StartLoadSampler()
{
    if (!g_cfg.loadSampler || g_thread) return;
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (k32) g_getThreadDescription = (PFN_GetThreadDescription)GetProcAddress(k32, "GetThreadDescription");
    g_thread = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr);
    Log("[loadsampler] started. Drive one session load, then quit, and read acevo_perf_load_samples.csv next to the exe.");
}

void StopLoadSampler()
{
    if (!g_thread) return;
    // At process exit Windows has already ended the sampler thread by the time DllMain runs, and DllMain's
    // detach is the only caller, so the flag stops nothing today and nothing waits for the thread. The
    // window since the last summary keeps its bucket counts in the CSV to the last whole second, and the
    // rest of its tables are lost.
    InterlockedExchange(&g_stop, 1);
    Log("[loadsampler] %llu samples in total", (unsigned long long)(g_grandTotal + g_total));
    if (g_csv != INVALID_HANDLE_VALUE) CloseHandle(g_csv);
    g_csv = INVALID_HANDLE_VALUE;
    CloseHandle(g_thread);
    g_thread = nullptr;
}
