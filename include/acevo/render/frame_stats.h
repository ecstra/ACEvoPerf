#pragma once
#include "acevo/common.h"

// Frame timing measured at IDXGISwapChain::Present. Counters are reset by the
// timeline thread every second, the frame buffer is drained by it too.
extern std::atomic<uint64_t> g_frames, g_frameSumUs, g_frameMaxUs, g_hitch20, g_hitchCfg;
extern std::atomic<int> g_hitchLogBudget;
extern CRITICAL_SECTION g_frameCs;

// One presented frame with the streaming requests enqueued since the previous one.
struct FrameSample {
    float t;            // seconds since attach
    float ms;           // time since the previous present
    float present;      // time the previous Present call itself took (blocked waiting)
    float wait;         // time the render thread spent in wait calls during this frame
    float fence;        // the part of it spent on events D3D12 fences signal (waiting for the GPU)
    float tileMap;      // time inside ID3D12CommandQueue::UpdateTileMappings in this frame
    float execute;      // time inside ID3D12CommandQueue::ExecuteCommandLists in this frame
    uint32_t mappedTiles; // tiles mapped or unmapped by UpdateTileMappings in this frame
    uint32_t tiles;     // texture tile requests
    uint32_t f2m;       // file to memory requests
    uint32_t gpumem;    // memory to GPU uploads
};
extern std::vector<FrameSample> g_frameBuf;

void InitFrameStats();          // QPC base and the frame buffer lock, call from DllMain
void InstallWaitHooks();        // time the render thread's waits, sorted by fence, latency object or other
void* OriginalExecuteCommandLists();   // the unhooked ID3D12CommandQueue::ExecuteCommandLists, null before the device exists
double NowSec();                // seconds since attach
void HookSwapChain(IUnknown* swapChain);
