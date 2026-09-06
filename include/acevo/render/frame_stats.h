#pragma once
#include "acevo/common.h"

// Frame timing measured at IDXGISwapChain::Present. Counters are reset by the
// timeline thread every second, the frame buffer is drained by it too.
extern std::atomic<uint64_t> g_frames, g_frameSumUs, g_frameMaxUs, g_hitch20, g_hitchCfg;
extern std::atomic<int> g_hitchLogBudget;
extern std::atomic<DWORD> g_presentThreadId;   // the thread that calls Present, the render thread
extern CRITICAL_SECTION g_frameCs;

// One presented frame with the streaming requests enqueued since the previous one.
struct FrameSample {
    float t;            // seconds since attach
    float ms;           // time since the previous present
    uint32_t tiles;     // texture tile requests
    uint32_t f2m;       // file to memory requests
    uint32_t gpumem;    // memory to GPU uploads
};
extern std::vector<FrameSample> g_frameBuf;

void InitFrameStats();          // QPC base and the frame buffer lock, call from DllMain
double NowSec();                // seconds since attach
void HookSwapChain(IUnknown* swapChain);
