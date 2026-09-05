#pragma once
#include "acevo/common.h"

// GPU time per frame, measured with timestamp queries submitted around every command list
// batch the game executes on the queue that presents. Diagnostics, `[profile] gpu_timing=1`.
struct GpuFrameRow {
    float t;            // the frame's present time, the same value the frames CSV carries
    uint32_t submits;   // command list batches the game executed in the frame
    float busyMs;       // GPU time inside those batches
    float spanMs;       // GPU time from the first batch's start to the last batch's end
    float lagMs;        // how far behind the CPU the GPU started the first batch
};

typedef void (STDMETHODCALLTYPE *PFN_ExecuteCommandListsOriginal)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

void GpuTimingSetPresentQueue(IUnknown* queue, PFN_ExecuteCommandListsOriginal execute);
bool GpuTimingIsPresentQueue(ID3D12CommandQueue* queue);
void GpuTimingMark(ID3D12CommandQueue* queue);      // one timestamp, called before and after a batch
void GpuTimingOnPresent(float frameT);              // close the frame's marks and harvest finished frames
void GpuTimingDrain(std::vector<GpuFrameRow>& out); // rows ready for the CSV
