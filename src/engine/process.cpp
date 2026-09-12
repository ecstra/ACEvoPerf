#include "acevo/engine/process.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"

typedef LONG (NTAPI *PFN_NtSetTimerResolution)(ULONG, BOOLEAN, PULONG);

// GPU scheduling priority. Windows gives every process a priority class for GPU work,
// separate from the CPU one, and nothing sets it for this game. It decides whose command
// buffers the GPU scheduler runs first when something else is also drawing, which on this
// machine includes the integrated adapter that owns the monitor and composites the desktop.
// The names and values are from d3dkmthk.h, reached through gdi32 so no driver headers.
enum D3DKMT_SCHEDULINGPRIORITYCLASS {
    D3DKMT_SCHEDULINGPRIORITYCLASS_IDLE = 0,
    D3DKMT_SCHEDULINGPRIORITYCLASS_BELOW_NORMAL = 1,
    D3DKMT_SCHEDULINGPRIORITYCLASS_NORMAL = 2,
    D3DKMT_SCHEDULINGPRIORITYCLASS_ABOVE_NORMAL = 3,
    D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH = 4,
    D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME = 5,
};
typedef LONG (APIENTRY *PFN_D3DKMTSetProcessSchedulingPriorityClass)(HANDLE, D3DKMT_SCHEDULINGPRIORITYCLASS);
typedef LONG (APIENTRY *PFN_D3DKMTGetProcessSchedulingPriorityClass)(HANDLE, D3DKMT_SCHEDULINGPRIORITYCLASS*);

static const char* GpuPriorityName(int v)
{
    switch (v) {
        case 0: return "idle";
        case 1: return "below normal";
        case 2: return "normal";
        case 3: return "above normal";
        case 4: return "high";
        case 5: return "realtime";
        default: return "?";
    }
}

static void ApplyGpuPriority()
{
    if (g_cfg.gpuPriority < 0) { Log("gpu scheduling priority: unchanged"); return; }

    HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    auto set = gdi ? (PFN_D3DKMTSetProcessSchedulingPriorityClass)GetProcAddress(gdi, "D3DKMTSetProcessSchedulingPriorityClass") : nullptr;
    auto get = gdi ? (PFN_D3DKMTGetProcessSchedulingPriorityClass)GetProcAddress(gdi, "D3DKMTGetProcessSchedulingPriorityClass") : nullptr;
    if (!set) { Log("gpu scheduling priority: D3DKMTSetProcessSchedulingPriorityClass not available"); return; }

    D3DKMT_SCHEDULINGPRIORITYCLASS before = D3DKMT_SCHEDULINGPRIORITYCLASS_NORMAL;
    if (get) get(GetCurrentProcess(), &before);

    D3DKMT_SCHEDULINGPRIORITYCLASS want = (D3DKMT_SCHEDULINGPRIORITYCLASS)g_cfg.gpuPriority;
    LONG status = set(GetCurrentProcess(), want);
    if (status < 0 && want == D3DKMT_SCHEDULINGPRIORITYCLASS_REALTIME) {
        // Realtime needs a privilege the game does not have, so drop one step rather than
        // leaving the process on whatever it had.
        Log("gpu scheduling priority: realtime refused (status 0x%08X), trying high", (unsigned)status);
        want = D3DKMT_SCHEDULINGPRIORITYCLASS_HIGH;
        status = set(GetCurrentProcess(), want);
    }
    D3DKMT_SCHEDULINGPRIORITYCLASS after = before;
    if (get) get(GetCurrentProcess(), &after);
    Log("gpu scheduling priority: %s -> %s (status 0x%08X)", GpuPriorityName(before), GpuPriorityName(after), (unsigned)status);
}

// Keep a floor under the working set so Windows cannot page the game out under memory
// pressure and then fault it back in mid corner. Off by default: a floor that is too high
// for the machine is refused, and one that is met by squeezing everything else is worse
// than the trimming it prevents.
static void ApplyWorkingSetFloor()
{
    if (g_cfg.workingSetFloorMb <= 0) return;

    MEMORYSTATUSEX ms = {}; ms.dwLength = sizeof ms;
    GlobalMemoryStatusEx(&ms);
    SIZE_T floor = (SIZE_T)g_cfg.workingSetFloorMb << 20;
    SIZE_T half = (SIZE_T)(ms.ullTotalPhys / 2);
    if (floor > half) {
        Log("working set floor: %d MB is more than half of this machine's memory, skipped", g_cfg.workingSetFloorMb);
        return;
    }
    BOOL ok = SetProcessWorkingSetSizeEx(GetCurrentProcess(), floor, floor * 4, QUOTA_LIMITS_HARDWS_MIN_ENABLE);
    Log("working set floor: %d MB -> %d (err %lu)", g_cfg.workingSetFloorMb, ok, ok ? 0 : GetLastError());
}

void ApplyProcessTweaks()
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

    ApplyGpuPriority();
    ApplyWorkingSetFloor();
}
