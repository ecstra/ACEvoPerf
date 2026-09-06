#include "acevo/render/gpu_timing.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"

namespace {

// Each mark is one tiny command list with a timestamp query resolved into a readback buffer,
// executed on the present queue right before or after a batch of the game's command lists.
// The GPU stamps it when it gets there, so before and after pairs bracket the batch's GPU
// time and the first mark's stamp against its submit time is the queue's lag.
const uint32_t kMarks = 512;
ID3D12CommandQueue* g_queue = nullptr;
PFN_ExecuteCommandListsOriginal g_execute = nullptr;
ID3D12QueryHeap* g_heap = nullptr;
ID3D12Resource* g_readback = nullptr;
const uint64_t* g_stamps = nullptr;
ID3D12Fence* g_fence = nullptr;
ID3D12CommandAllocator* g_allocators[kMarks];
ID3D12GraphicsCommandList* g_lists[kMarks];
int64_t g_cpuSubmit[kMarks];
uint64_t g_markWrite = 0;
uint64_t g_frameFirstMark = 0;
uint64_t g_gpuFreq = 0, g_gpuCal = 0, g_cpuCal = 0;
double g_lastCalibration = 0.0;
LARGE_INTEGER g_qpf;
bool g_ready = false;
CRITICAL_SECTION g_cs;

struct PendingFrame { float t; uint64_t firstMark; uint64_t endMark; };
std::vector<PendingFrame> g_pending;
std::vector<GpuFrameRow> g_rows;
uint64_t g_previousFrameEnd = 0;   // GPU stamp of the previous frame's last batch end

bool CreateResources(ID3D12Device* device)
{
    D3D12_QUERY_HEAP_DESC heap = { D3D12_QUERY_HEAP_TYPE_TIMESTAMP, kMarks, 0 };
    if (FAILED(device->CreateQueryHeap(&heap, __uuidof(ID3D12QueryHeap), (void**)&g_heap))) return false;

    D3D12_HEAP_PROPERTIES props = {};
    props.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer = {};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = kMarks * sizeof(uint64_t);
    buffer.Height = 1; buffer.DepthOrArraySize = 1; buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource), (void**)&g_readback))) return false;
    void* mapped = nullptr;
    if (FAILED(g_readback->Map(0, nullptr, &mapped))) return false;
    g_stamps = (const uint64_t*)mapped;

    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&g_fence))) return false;
    for (uint32_t i = 0; i < kMarks; ++i) {
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&g_allocators[i]))) return false;
        if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_allocators[i], nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&g_lists[i]))) return false;
        g_lists[i]->Close();
    }
    return true;
}

void Calibrate()
{
    uint64_t gpu = 0, cpu = 0;
    if (SUCCEEDED(g_queue->GetClockCalibration(&gpu, &cpu))) { g_gpuCal = gpu; g_cpuCal = cpu; }
}

double GpuToCpuQpc(uint64_t gpuStamp)
{
    return (double)g_cpuCal + ((double)gpuStamp - (double)g_gpuCal) * (double)g_qpf.QuadPart / (double)g_gpuFreq;
}

} // namespace

void GpuTimingSetPresentQueue(IUnknown* queueUnknown, PFN_ExecuteCommandListsOriginal execute)
{
    if (!g_cfg.gpuTiming || g_queue || !queueUnknown || !execute) return;
    ID3D12CommandQueue* queue = nullptr;
    if (FAILED(queueUnknown->QueryInterface(__uuidof(ID3D12CommandQueue), (void**)&queue)) || !queue) return;
    ID3D12Device* device = nullptr;
    if (FAILED(queue->GetDevice(__uuidof(ID3D12Device), (void**)&device)) || !device) { queue->Release(); return; }
    InitializeCriticalSection(&g_cs);
    QueryPerformanceFrequency(&g_qpf);
    g_queue = queue;
    g_execute = execute;
    bool ok = CreateResources(device) && SUCCEEDED(queue->GetTimestampFrequency(&g_gpuFreq)) && g_gpuFreq;
    device->Release();
    if (ok) Calibrate();
    g_ready = ok;
    Log("gpu timing: present queue %p, %u timestamp marks, GPU clock %llu Hz, %s", queue, kMarks, (unsigned long long)g_gpuFreq, ok ? "ready" : "failed");
}

bool GpuTimingIsPresentQueue(ID3D12CommandQueue* queue)
{
    return g_ready && queue == g_queue;
}

void GpuTimingMark(ID3D12CommandQueue* queue)
{
    if (!g_ready || queue != g_queue) return;
    EnterCriticalSection(&g_cs);
    uint64_t index = g_markWrite;
    uint32_t slot = (uint32_t)(index % kMarks);
    // the slot's list was used kMarks marks ago, the GPU is long done with it
    while (index >= kMarks && g_fence->GetCompletedValue() < index - kMarks + 1) SwitchToThread();
    g_allocators[slot]->Reset();
    g_lists[slot]->Reset(g_allocators[slot], nullptr);
    g_lists[slot]->EndQuery(g_heap, D3D12_QUERY_TYPE_TIMESTAMP, slot);
    g_lists[slot]->ResolveQueryData(g_heap, D3D12_QUERY_TYPE_TIMESTAMP, slot, 1, g_readback, slot * sizeof(uint64_t));
    g_lists[slot]->Close();
    ID3D12CommandList* lists[1] = { g_lists[slot] };
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    g_cpuSubmit[slot] = now.QuadPart;
    g_execute(queue, 1, lists);
    g_queue->Signal(g_fence, index + 1);
    g_markWrite = index + 1;
    LeaveCriticalSection(&g_cs);
}

void GpuTimingOnPresent(float frameT)
{
    if (!g_ready) return;
    EnterCriticalSection(&g_cs);
    if (g_markWrite > g_frameFirstMark) {
        if (g_pending.size() >= 64) g_pending.erase(g_pending.begin());
        g_pending.push_back({ frameT, g_frameFirstMark, g_markWrite });
    }
    g_frameFirstMark = g_markWrite;

    uint64_t done = g_fence->GetCompletedValue();
    while (!g_pending.empty() && g_pending.front().endMark <= done) {
        PendingFrame f = g_pending.front();
        g_pending.erase(g_pending.begin());
        uint64_t first = g_stamps[f.firstMark % kMarks];
        uint64_t last = g_stamps[(f.endMark - 1) % kMarks];
        const double gpuMs = 1000.0 / (double)g_gpuFreq;
        GpuFrameRow row = {};
        row.t = f.t;
        row.submits = (uint32_t)((f.endMark - f.firstMark) / 2);
        uint64_t busy = 0, previousEnd = g_previousFrameEnd;
        int batch = 0;
        for (uint64_t m = f.firstMark; m + 1 < f.endMark; m += 2, ++batch) {
            uint64_t a = g_stamps[m % kMarks], b = g_stamps[(m + 1) % kMarks];
            if (b > a) busy += b - a;
            if (batch < kBatchesPerRow) {
                row.batchMs[batch] = (float)((b > a ? b - a : 0) * gpuMs);
                row.gapMs[batch] = (float)((previousEnd && a > previousEnd ? a - previousEnd : 0) * gpuMs);
                row.batchLagMs[batch] = (float)((GpuToCpuQpc(a) - (double)g_cpuSubmit[m % kMarks]) * 1000.0 / (double)g_qpf.QuadPart);
            }
            previousEnd = b;
        }
        g_previousFrameEnd = last;
        double lagQpc = GpuToCpuQpc(first) - (double)g_cpuSubmit[f.firstMark % kMarks];
        row.busyMs = (float)(busy * gpuMs);
        row.spanMs = (float)((last > first ? last - first : 0) * gpuMs);
        row.lagMs = (float)(lagQpc * 1000.0 / (double)g_qpf.QuadPart);
        if (g_rows.size() < 100000) g_rows.push_back(row);
    }
    if (frameT - g_lastCalibration > 10.0f) { Calibrate(); g_lastCalibration = frameT; }
    LeaveCriticalSection(&g_cs);
}

void GpuTimingDrain(std::vector<GpuFrameRow>& out)
{
    if (!g_ready) return;
    EnterCriticalSection(&g_cs);
    out.swap(g_rows);
    LeaveCriticalSection(&g_cs);
}
