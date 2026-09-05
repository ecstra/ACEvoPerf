#pragma once
#include "acevo/common.h"

// Frame timing measured at IDXGISwapChain::Present. Counters are reset by the
// timeline thread every second, the frame buffer is drained by it too.
extern std::atomic<uint64_t> g_frames, g_frameSumUs, g_frameMaxUs, g_hitch20, g_hitchCfg;
extern std::atomic<int> g_hitchLogBudget;
extern CRITICAL_SECTION g_frameCs;
extern std::vector<std::pair<float, float>> g_frameBuf;   // (seconds since attach, frame ms)

void InitFrameStats();          // QPC base and the frame buffer lock, call from DllMain
double NowSec();                // seconds since attach
void HookSwapChain(IUnknown* swapChain);
