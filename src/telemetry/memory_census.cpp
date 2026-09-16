// Memory census, see include/acevo/telemetry/memory_census.h.
//
// Committed memory reaches the process three ways, and each census counts all three:
//
//   - VirtualAlloc and VirtualAlloc2, which engine allocators, V8, the driver and most libraries use
//     for big blocks. Their import slots are patched in every module, and each commit is remembered
//     with the three return addresses above the call, which names the code that asked for it.
//     Modules that load later are patched at the next census.
//   - Heaps, which ntdll grows without going through those imports, read with HeapSummary.
//   - Everything else private, found by walking the address space.
//
// A remembered range is checked against the address space at every census instead of hooking the
// frees, so a range that was released, or decommitted in part, only counts what is still committed.
//
// HeapSummary walks a heap under its lock, and the game's main heap holds 4 to 7 GB, so reading it
// froze the game for 200 ms in the menu and 1.4 s on track when the census ran every ten seconds.
// A census now runs once the committed total has settled, at start and after each track unloads,
// which is the menu the owner waits in. It then compacts every heap and reads them again, which
// tells memory the game still uses apart from memory it freed and the heap kept. HeapCompact is the
// call that gives it back, in a harness 71 MB of freed small blocks went down to 2 MB, where
// HeapOptimizeResources returned nothing.
#include "acevo/telemetry/memory_census.h"
#include "acevo/core/config.h"
#include "acevo/core/iat.h"
#include "acevo/core/log.h"
#include "acevo/render/frame_stats.h"
#include <deque>
#include <map>
#include <unordered_map>

namespace {

constexpr int kStackFrames = 3;
constexpr size_t kPage = 0x1000;
constexpr size_t kSiteRowBytes = 4ull << 20;
constexpr size_t kHeapRowBytes = 32ull << 20;
constexpr double kMb = 1024.0 * 1024.0;

// Settled means the commit charge moved less than this over the last kSettleSeconds, and a track
// unload is a fall of at least kUnloadMb from the highest reading since the previous census. The
// Red Bull Ring gives back about 0.9 GB when it unloads.
constexpr double kSettleBandMb = 150.0;
constexpr size_t kSettleSeconds = 15;
constexpr double kUnloadMb = 700.0;

struct CallSite {
    uintptr_t frames[kStackFrames];
    bool operator==(const CallSite& other) const { return memcmp(frames, other.frames, sizeof frames) == 0; }
};

struct CallSiteHash {
    size_t operator()(const CallSite& site) const
    {
        size_t hash = 0;
        for (uintptr_t frame : site.frames) hash = hash * 1000003 ^ std::hash<uintptr_t>()(frame);
        return hash;
    }
};

struct Committed {
    size_t bytes;
    uint32_t site;
};

using PFN_VirtualAlloc = LPVOID (WINAPI*)(LPVOID, SIZE_T, DWORD, DWORD);
using PFN_VirtualAlloc2 = PVOID (WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, void*, ULONG);

PFN_VirtualAlloc g_origVirtualAlloc = nullptr;
PFN_VirtualAlloc2 g_origVirtualAlloc2 = nullptr;

SRWLOCK g_lock = SRWLOCK_INIT;
std::map<uintptr_t, Committed> g_ranges;
std::vector<CallSite> g_sites;
std::unordered_map<CallSite, uint32_t, CallSiteHash> g_siteIds;
thread_local bool t_remembering = false;

bool g_installed = false;
HANDLE g_csv = INVALID_HANDLE_VALUE;
std::deque<double> g_recentMb;
double g_highestMb = 0;
bool g_baselineTaken = false;

__declspec(noinline) void Remember(PVOID result, PVOID requested, SIZE_T size, void* caller)
{
    if (t_remembering) return;
    t_remembering = true;

    void* stack[8] = {};
    USHORT depth = RtlCaptureStackBackTrace(0, 8, stack, nullptr);
    CallSite site = {};
    site.frames[0] = (uintptr_t)caller;
    for (USHORT i = 0; i < depth; ++i) {
        if (stack[i] != caller) continue;
        for (int f = 1; f < kStackFrames && i + f < depth; ++f) site.frames[f] = (uintptr_t)stack[i + f];
        break;
    }

    uintptr_t start = (uintptr_t)result;
    uintptr_t end = ((uintptr_t)(requested ? requested : result) + size + kPage - 1) & ~(uintptr_t)(kPage - 1);
    size_t bytes = end > start ? end - start : 0;

    AcquireSRWLockExclusive(&g_lock);
    auto [id, fresh] = g_siteIds.try_emplace(site, (uint32_t)g_sites.size());
    if (fresh) g_sites.push_back(site);

    // A commit inside a range already remembered is a recommit and keeps the first owner. One that
    // overlaps older ranges replaces them, those are gone or being reused.
    auto next = g_ranges.upper_bound(start);
    bool covered = false;
    if (next != g_ranges.begin()) {
        auto prev = std::prev(next);
        uintptr_t prevEnd = prev->first + prev->second.bytes;
        if (prevEnd >= end) covered = true;
        else if (prevEnd > start) prev->second.bytes = start - prev->first;
    }
    if (!covered && bytes) {
        while (next != g_ranges.end() && next->first < end) next = g_ranges.erase(next);
        g_ranges[start] = Committed{ bytes, id->second };
    }
    ReleaseSRWLockExclusive(&g_lock);

    t_remembering = false;
}

LPVOID WINAPI Hook_VirtualAlloc(LPVOID address, SIZE_T size, DWORD type, DWORD protect)
{
    LPVOID result = g_origVirtualAlloc(address, size, type, protect);
    if (result && (type & MEM_COMMIT)) Remember(result, address, size, _ReturnAddress());
    return result;
}

PVOID WINAPI Hook_VirtualAlloc2(HANDLE process, PVOID address, SIZE_T size, ULONG type, ULONG protect, void* parameters, ULONG count)
{
    PVOID result = g_origVirtualAlloc2(process, address, size, type, protect, parameters, count);
    bool self = !process || process == GetCurrentProcess() || GetProcessId(process) == GetCurrentProcessId();
    if (result && self && (type & MEM_COMMIT)) Remember(result, address, size, _ReturnAddress());
    return result;
}

size_t CommittedPrivate(uintptr_t start, size_t bytes)
{
    size_t live = 0;
    uintptr_t end = start + bytes;
    for (uintptr_t at = start; at < end;) {
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery((LPCVOID)at, &info, sizeof info)) break;
        uintptr_t regionEnd = (uintptr_t)info.BaseAddress + info.RegionSize;
        if (info.State == MEM_COMMIT && info.Type == MEM_PRIVATE) live += std::min(regionEnd, end) - at;
        at = regionEnd;
    }
    return live;
}

// A heap created without serialisation by another component is not safe to touch while it is in
// use, so a fault on one only skips it.
bool ReadHeap(HANDLE heap, HEAP_SUMMARY* summary)
{
    __try {
        return HeapSummary(heap, 0, summary) != FALSE;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void CompactHeap(HANDLE heap)
{
    __try {
        HeapCompact(heap, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void CompactHeaps()
{
    std::vector<HANDLE> heaps(GetProcessHeaps(0, nullptr) + 16);
    DWORD count = std::min<DWORD>(GetProcessHeaps((DWORD)heaps.size(), heaps.data()), (DWORD)heaps.size());
    for (DWORD i = 0; i < count; ++i) CompactHeap(heaps[i]);
}

struct HeapReading {
    HANDLE heap;
    HEAP_SUMMARY summary;
};

struct HeapTotals {
    size_t committed = 0;
    size_t inUse = 0;
    std::vector<HeapReading> big;
};

HeapTotals ReadHeaps()
{
    HeapTotals totals;
    std::vector<HANDLE> heaps(GetProcessHeaps(0, nullptr) + 16);
    DWORD count = std::min<DWORD>(GetProcessHeaps((DWORD)heaps.size(), heaps.data()), (DWORD)heaps.size());
    for (DWORD i = 0; i < count; ++i) {
        HEAP_SUMMARY summary = {};
        summary.cb = sizeof summary;
        if (!ReadHeap(heaps[i], &summary)) continue;
        totals.committed += summary.cbCommitted;
        totals.inUse += summary.cbAllocated;
        if (summary.cbCommitted >= kHeapRowBytes) totals.big.push_back({ heaps[i], summary });
    }
    return totals;
}

double CommitChargeMb()
{
    PROCESS_MEMORY_COUNTERS_EX counters = {};
    counters.cb = sizeof counters;
    K32GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&counters, sizeof counters);
    return counters.PrivateUsage / kMb;
}

double SecondsSince(const LARGE_INTEGER& start)
{
    LARGE_INTEGER now, frequency;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&frequency);
    return double(now.QuadPart - start.QuadPart) / frequency.QuadPart;
}

void DescribeFrame(uintptr_t address, char* out, size_t size)
{
    if (!address) {
        out[0] = 0;
        return;
    }
    HMODULE module = nullptr;
    char path[MAX_PATH] = "";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)address, &module)
        && GetModuleFileNameA(module, path, sizeof path)) {
        const char* name = strrchr(path, '\\');
        _snprintf_s(out, size, _TRUNCATE, "%s+0x%llX", name ? name + 1 : path, (unsigned long long)(address - (uintptr_t)module));
        return;
    }
    _snprintf_s(out, size, _TRUNCATE, "0x%llX", (unsigned long long)address);
}

void WriteRow(const char* format, ...)
{
    char line[512];
    int n = _snprintf_s(line, sizeof line, _TRUNCATE, "%.1f,", NowSec());
    va_list args;
    va_start(args, format);
    int m = _vsnprintf_s(line + n, sizeof line - n - 2, _TRUNCATE, format, args);
    va_end(args);
    if (m < 0) m = (int)strlen(line + n);
    n += m;
    line[n++] = '\r';
    line[n++] = '\n';
    DWORD written = 0;
    WriteFile(g_csv, line, (DWORD)n, &written, nullptr);
}

void Census(const char* when)
{
    LARGE_INTEGER started;
    QueryPerformanceCounter(&started);

    // Copied out under the lock and queried without it, the game keeps allocating meanwhile.
    AcquireSRWLockShared(&g_lock);
    std::vector<std::pair<uintptr_t, Committed>> ranges(g_ranges.begin(), g_ranges.end());
    std::vector<CallSite> sites = g_sites;
    ReleaseSRWLockShared(&g_lock);

    struct Live {
        size_t bytes = 0;
        uint32_t ranges = 0;
    };
    std::vector<Live> live(sites.size());
    std::vector<uintptr_t> gone;
    size_t remembered = 0;
    for (auto& [start, range] : ranges) {
        size_t bytes = CommittedPrivate(start, range.bytes);
        if (!bytes) {
            gone.push_back(start);
            continue;
        }
        live[range.site].bytes += bytes;
        live[range.site].ranges++;
        remembered += bytes;
    }
    if (!gone.empty()) {
        AcquireSRWLockExclusive(&g_lock);
        for (uintptr_t start : gone) {
            auto it = g_ranges.find(start);
            if (it != g_ranges.end() && !CommittedPrivate(start, it->second.bytes)) g_ranges.erase(it);
        }
        ReleaseSRWLockExclusive(&g_lock);
    }

    SYSTEM_INFO system;
    GetSystemInfo(&system);
    size_t privateBytes = 0, mappedBytes = 0, imageBytes = 0;
    uint32_t regions = 0;
    for (uintptr_t at = (uintptr_t)system.lpMinimumApplicationAddress; at < (uintptr_t)system.lpMaximumApplicationAddress;) {
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery((LPCVOID)at, &info, sizeof info)) break;
        if (info.State == MEM_COMMIT) {
            if (info.Type == MEM_PRIVATE) privateBytes += info.RegionSize;
            else if (info.Type == MEM_MAPPED) mappedBytes += info.RegionSize;
            else if (info.Type == MEM_IMAGE) imageBytes += info.RegionSize;
        }
        ++regions;
        at = (uintptr_t)info.BaseAddress + info.RegionSize;
    }

    double chargeMb = CommitChargeMb();
    HeapTotals heaps = ReadHeaps();

    WriteRow("settled,%s,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%u", when, chargeMb, privateBytes / kMb, heaps.committed / kMb,
        heaps.inUse / kMb, remembered / kMb, mappedBytes / kMb, imageBytes / kMb, regions);
    for (auto& reading : heaps.big)
        WriteRow("heap,%s,%p,%.0f,%.0f,%.0f,,,,", when, reading.heap, reading.summary.cbCommitted / kMb,
            reading.summary.cbAllocated / kMb, reading.summary.cbReserved / kMb);

    std::vector<uint32_t> order;
    for (uint32_t id = 0; id < live.size(); ++id)
        if (live[id].bytes >= kSiteRowBytes) order.push_back(id);
    std::sort(order.begin(), order.end(), [&live](uint32_t a, uint32_t b) { return live[a].bytes > live[b].bytes; });
    for (uint32_t id : order) {
        char frames[kStackFrames][96];
        for (int f = 0; f < kStackFrames; ++f) DescribeFrame(sites[id].frames[f], frames[f], sizeof frames[f]);
        WriteRow("site,%s,%.1f,%u,%s,%s,%s,,,", when, live[id].bytes / kMb, live[id].ranges, frames[0], frames[1], frames[2]);
    }
    double readSeconds = SecondsSince(started);

    // What the heaps give back when compacted is memory the game had already freed.
    LARGE_INTEGER compacting;
    QueryPerformanceCounter(&compacting);
    CompactHeaps();
    double compactSeconds = SecondsSince(compacting);
    double afterMb = CommitChargeMb();
    HeapTotals after = ReadHeaps();

    WriteRow("compacted,%s,%.2f,%.0f,%.0f,%.0f,%.0f,%.2f,,", when, compactSeconds, afterMb, after.committed / kMb, after.inUse / kMb,
        chargeMb - afterMb, readSeconds);
    Log("[memory] census %s: commit %.0f MB, heaps %.0f MB committed of which %.0f MB in use, VirtualAlloc calls still holding %.0f MB, "
        "other private %.0f MB, mapped %.0f MB. After compacting the heaps (%.2f s): commit %.0f MB, heaps %.0f MB committed, %.0f MB "
        "returned. Reading took %.2f s.",
        when, chargeMb, heaps.committed / kMb, heaps.inUse / kMb, remembered / kMb,
        (double)(privateBytes > heaps.committed + remembered ? privateBytes - heaps.committed - remembered : 0) / kMb, mappedBytes / kMb,
        compactSeconds, afterMb, after.committed / kMb, chargeMb - afterMb, readSeconds);
}

} // namespace

void InstallMemoryCensus()
{
    if (!g_cfg.memoryCensus) return;

    int slots = PatchEverywhere("VirtualAlloc", (void*)&Hook_VirtualAlloc, (void**)&g_origVirtualAlloc);
    int slots2 = PatchEverywhere("VirtualAlloc2", (void*)&Hook_VirtualAlloc2, (void**)&g_origVirtualAlloc2);
    if (!g_origVirtualAlloc) {
        Log("[memory] VirtualAlloc not found, no census");
        return;
    }

    std::wstring path = g_dir + L"acevo_perf_memory.csv";
    g_csv = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_csv == INVALID_HANDLE_VALUE) {
        Log("[memory] could not create acevo_perf_memory.csv, no census");
        return;
    }
    const char* header = "t_s,kind,when,a,b,c,d,e,f,g,h\r\n";
    DWORD written = 0;
    WriteFile(g_csv, header, (DWORD)strlen(header), &written, nullptr);

    g_installed = true;
    Log("[memory] census on, VirtualAlloc hooked in %d import slots and VirtualAlloc2 in %d. A census runs once the commit charge has "
        "settled for %zu s, at start and after each track unloads, into acevo_perf_memory.csv and a [memory] line here.",
        slots, slots2, kSettleSeconds);
}

void MemoryCensusTick()
{
    if (!g_installed) return;

    double chargeMb = CommitChargeMb();
    g_recentMb.push_back(chargeMb);
    if (g_recentMb.size() > kSettleSeconds) g_recentMb.pop_front();
    g_highestMb = std::max(g_highestMb, chargeMb);
    if (g_recentMb.size() < kSettleSeconds) return;

    auto [lowest, highest] = std::minmax_element(g_recentMb.begin(), g_recentMb.end());
    if (*highest - *lowest > kSettleBandMb) return;

    bool unloaded = g_highestMb - chargeMb >= kUnloadMb;
    if (g_baselineTaken && !unloaded) return;

    // DLLs the game loaded since the last census, the driver among them, get their slots patched.
    PatchEverywhere("VirtualAlloc", (void*)&Hook_VirtualAlloc, (void**)&g_origVirtualAlloc);
    if (g_origVirtualAlloc2) PatchEverywhere("VirtualAlloc2", (void*)&Hook_VirtualAlloc2, (void**)&g_origVirtualAlloc2);

    Census(g_baselineTaken ? "after an unload" : "at start");
    g_baselineTaken = true;
    g_recentMb.clear();
    g_highestMb = CommitChargeMb();
}
