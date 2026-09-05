#include "acevo/engine/process.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"

typedef LONG (NTAPI *PFN_NtSetTimerResolution)(ULONG, BOOLEAN, PULONG);

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
}
