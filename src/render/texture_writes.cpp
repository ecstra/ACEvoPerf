#include "acevo/render/texture_writes.h"
#include "acevo/core/config.h"
#include "acevo/core/log.h"
#include "acevo/render/frame_stats.h"
#include "acevo/telemetry/streaming_trace.h"
#include <unordered_map>

// The reload fix keeps a texture's finer mip that the streamer would have dropped and later read
// back from the package. That is only safe if nothing else writes into streamed textures at
// runtime. If the dynamic track, multiplayer or anything else painted into one, a reload would
// reset that content, and holding the mip would keep it longer.
//
// Content can reach a texture on the GPU in few ways, and this watches all of them:
//
//   - A shader writes it through an unordered access or render target view. D3D12 refuses both
//     views unless the resource was created with D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS or
//     ALLOW_RENDER_TARGET, so the creation flags of every streamed texture say whether it can be
//     written this way at all. They are read at every tile request, since a texture freed and a
//     new one created at its address would otherwise never be checked.
//   - A command list copies into it. CopyTextureRegion, CopyResource, CopyTiles from a buffer and
//     ResolveSubresource are hooked, vtable slots 16 to 19 of ID3D12GraphicsCommandList.
//   - The CPU cannot map a reserved texture, and its tiles only ever change mapping through the
//     engine's own load path, which is followed by the DirectStorage upload the trace records.
//
// DirectStorage does its own uploads with the same command list calls from its own module, so
// every copy is attributed to the module that made the call. Uploads by the DirectStorage core
// are the streaming itself. A copy into a streamed texture from the game's exe or anywhere else
// is the runtime write this is looking for. Copies into resources the tile queue never streamed
// are counted too, as proof the hooks run.

namespace {

const int kSlotCopyTextureRegion = 16;
const int kSlotCopyResource = 17;
const int kSlotCopyTiles = 18;
const int kSlotResolveSubresource = 19;

using PFN_CopyTextureRegion = void (STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, const D3D12_TEXTURE_COPY_LOCATION*, UINT, UINT, UINT, const D3D12_TEXTURE_COPY_LOCATION*, const D3D12_BOX*);
using PFN_CopyResource = void (STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*);
using PFN_CopyTiles = HRESULT (STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12Resource*, const D3D12_TILED_RESOURCE_COORDINATE*, const D3D12_TILE_REGION_SIZE*, ID3D12Resource*, UINT64, D3D12_TILE_COPY_FLAGS);
using PFN_ResolveSubresource = void (STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12Resource*, UINT, ID3D12Resource*, UINT, DXGI_FORMAT);

PFN_CopyTextureRegion g_origCopyTextureRegion = nullptr;
PFN_CopyResource g_origCopyResource = nullptr;
PFN_CopyTiles g_origCopyTiles = nullptr;
PFN_ResolveSubresource g_origResolveSubresource = nullptr;

struct Streamed {
    double since;
    D3D12_RESOURCE_FLAGS flags;
    D3D12_TEXTURE_LAYOUT layout;
};

SRWLOCK g_lock = SRWLOCK_INIT;
std::unordered_map<ID3D12Resource*, Streamed> g_streamed;
std::atomic<bool> g_hooked{false};

std::atomic<uint64_t> g_streamedUav{0}, g_streamedRtv{0}, g_streamedTiled{0};
std::atomic<uint64_t> g_byDirectStorage{0}, g_byGame{0}, g_byOther{0}, g_intoOther{0}, g_reusedAddress{0};
std::atomic<uint64_t> g_regionHits{0}, g_resourceHits{0}, g_tilesHits{0}, g_resolveHits{0};

struct ModuleRange {
    uintptr_t base = 0;
    uintptr_t end = 0;
};

ModuleRange g_exe;
std::atomic<uintptr_t> g_dsBase[2] = {}, g_dsEnd[2] = {};

ModuleRange RangeOf(HMODULE module)
{
    ModuleRange range;
    MODULEINFO info = {};
    if (module && GetModuleInformation(GetCurrentProcess(), module, &info, sizeof info)) {
        range.base = (uintptr_t)info.lpBaseOfDll;
        range.end = range.base + info.SizeOfImage;
    }
    return range;
}

// Never resolved from the copy hook, because both calls in here take the loader lock and that
// hook runs about 290 times a second during a load. It used to look them up on every call until
// it found them, on the reasoning that the cores load after the hooks go in. They do not: across
// the sessions on disk the core loads 302 to 846 ms before the first swap chain, and with
// bundled_runtime=0 the bundled one never loads at all, so a base of zero was indistinguishable
// from not looked up yet and the lookup repeated for the life of the run.
//
// Called where the hooks are installed and again from the once a second tick while either range
// is still missing. The tick is what keeps the old loop's one virtue: a core that loads after the
// hooks, which no session shows but nothing guarantees, is still picked up. Without it every
// DirectStorage copy for the rest of the run would be counted as a write by something else, which
// is the number DEC-017 rests on being zero.
static void ResolveCoreRanges()
{
    static const wchar_t* kCores[2] = { L"acevo_dstoragecore.dll", L"dstoragecore.dll" };
    for (int i = 0; i < 2; ++i) {
        ModuleRange range = RangeOf(GetModuleHandleW(kCores[i]));
        g_dsEnd[i].store(range.end);
        g_dsBase[i].store(range.base);
    }
}

bool FromDirectStorage(uintptr_t address)
{
    for (int i = 0; i < 2; ++i)
        if (g_dsBase[i].load() && address >= g_dsBase[i].load() && address < g_dsEnd[i].load()) return true;
    return false;
}

// Counts the write and reports it when it lands on a texture the tile queue streams from anywhere
// but DirectStorage. A resource freed and a new one allocated at the same address is excluded by
// the layout test below and by nothing else. The `since` stamp is carried into the row for the
// reader, never compared, so a new reserved tiled texture at a freed one's address still passes.
void NoteWrite(ID3D12Resource* target, const char* how, std::atomic<uint64_t>& hits, uintptr_t caller)
{
    if (!target) return;

    bool streamed = false;
    double since = 0;
    AcquireSRWLockShared(&g_lock);
    auto it = g_streamed.find(target);
    if (it != g_streamed.end()) {
        streamed = true;
        since = it->second.since;
    }
    ReleaseSRWLockShared(&g_lock);

    if (!streamed) {
        g_intoOther++;
        return;
    }

    // A streamed texture is reserved and tiled. An untiled destination at a recorded address is a
    // new resource that took the address after the texture was freed, the first run on 2026-09-13
    // caught 57 such copies into 64 by 64 BGRA textures.
    D3D12_RESOURCE_DESC desc = target->GetDesc();
    if (desc.Layout != D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE && desc.Layout != D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE) {
        g_reusedAddress++;
        return;
    }
    if (FromDirectStorage(caller)) {
        g_byDirectStorage++;
        return;
    }

    bool fromGame = caller >= g_exe.base && caller < g_exe.end;
    (fromGame ? g_byGame : g_byOther)++;
    hits++;
    TraceRow("write", "%s,%p,%.3f,%s,0x%llX,%llu,%u,%u,%d,0x%X", how, (void*)target, since, fromGame ? "exe" : "other",
        (unsigned long long)(fromGame ? caller - g_exe.base : caller), (unsigned long long)desc.Width, desc.Height,
        (unsigned)desc.Format, (int)desc.Layout, (unsigned)desc.Flags);
}

// Several vtables share the same functions, so one saved original serves them all. A slot that
// already holds the hook is left alone, and one holding anything but the saved original is not
// touched, since a layer in between would otherwise be skipped.
int PatchSlot(void** vt, int slot, void* hook, void** orig)
{
    if (vt[slot] == hook) return 1;
    if (*orig && vt[slot] != *orig) return 0;

    DWORD old = 0;
    if (!VirtualProtect(&vt[slot], sizeof(void*), PAGE_READWRITE, &old)) return 0;
    if (!*orig) *orig = vt[slot];
    vt[slot] = hook;
    VirtualProtect(&vt[slot], sizeof(void*), old, &old);
    return 1;
}

void STDMETHODCALLTYPE Hook_CopyTextureRegion(ID3D12GraphicsCommandList* self, const D3D12_TEXTURE_COPY_LOCATION* dst,
    UINT x, UINT y, UINT z, const D3D12_TEXTURE_COPY_LOCATION* src, const D3D12_BOX* box)
{
    if (dst) NoteWrite(dst->pResource, "region", g_regionHits, (uintptr_t)_ReturnAddress());
    g_origCopyTextureRegion(self, dst, x, y, z, src, box);
}

void STDMETHODCALLTYPE Hook_CopyResource(ID3D12GraphicsCommandList* self, ID3D12Resource* dst, ID3D12Resource* src)
{
    NoteWrite(dst, "resource", g_resourceHits, (uintptr_t)_ReturnAddress());
    g_origCopyResource(self, dst, src);
}

HRESULT STDMETHODCALLTYPE Hook_CopyTiles(ID3D12GraphicsCommandList* self, ID3D12Resource* tiled,
    const D3D12_TILED_RESOURCE_COORDINATE* start, const D3D12_TILE_REGION_SIZE* size, ID3D12Resource* buffer,
    UINT64 offset, D3D12_TILE_COPY_FLAGS flags)
{
    if (flags & D3D12_TILE_COPY_FLAG_LINEAR_BUFFER_TO_SWIZZLED_TILED_RESOURCE) NoteWrite(tiled, "tiles", g_tilesHits, (uintptr_t)_ReturnAddress());
    return g_origCopyTiles(self, tiled, start, size, buffer, offset, flags);
}

void STDMETHODCALLTYPE Hook_ResolveSubresource(ID3D12GraphicsCommandList* self, ID3D12Resource* dst, UINT dstSub,
    ID3D12Resource* src, UINT srcSub, DXGI_FORMAT format)
{
    NoteWrite(dst, "resolve", g_resolveHits, (uintptr_t)_ReturnAddress());
    g_origResolveSubresource(self, dst, dstSub, src, srcSub, format);
}

} // namespace

void NoteStreamedResource(ID3D12Resource* resource)
{
    if (!g_cfg.streamingTrace || !resource) return;

    D3D12_RESOURCE_DESC desc = resource->GetDesc();
    auto sameAsRecorded = [&desc](const Streamed& recorded) {
        return recorded.flags == desc.Flags && recorded.layout == desc.Layout;
    };

    AcquireSRWLockShared(&g_lock);
    auto found = g_streamed.find(resource);
    bool recorded = found != g_streamed.end() && sameAsRecorded(found->second);
    ReleaseSRWLockShared(&g_lock);
    if (recorded) return;

    AcquireSRWLockExclusive(&g_lock);
    auto [entry, inserted] = g_streamed.try_emplace(resource, Streamed{ NowSec(), desc.Flags, desc.Layout });
    bool recordedMeanwhile = !inserted && sameAsRecorded(entry->second);
    if (!inserted && !recordedMeanwhile) entry->second = Streamed{ NowSec(), desc.Flags, desc.Layout };
    ReleaseSRWLockExclusive(&g_lock);
    if (recordedMeanwhile) return;

    if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) g_streamedUav++;
    if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) g_streamedRtv++;
    // a reserved texture has to use one of the two 64 KB tile layouts, so this should match the
    // streamed count and confirms these are the tile queue's targets
    if (desc.Layout == D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE || desc.Layout == D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE) g_streamedTiled++;
}

void TextureWritesOnSwapChain(IUnknown* deviceOrQueue)
{
    if (!g_cfg.streamingTrace || !deviceOrQueue) return;

    // The queue check comes before the flag is taken. Taking it first and handing it back on a
    // failure lets a D3D11 swap chain on one thread hold it long enough for the game's D3D12 one
    // on another to see it taken and walk away, leaving nothing hooked at all.
    ID3D12CommandQueue* queue = nullptr;
    if (FAILED(deviceOrQueue->QueryInterface(__uuidof(ID3D12CommandQueue), (void**)&queue)) || !queue) return;
    if (g_hooked.exchange(true)) { queue->Release(); return; }

    ID3D12Device* device = nullptr;
    if (FAILED(queue->GetDevice(IID_PPV_ARGS(&device))) || !device) {
        g_hooked = false;
        queue->Release();
        return;
    }

    g_exe = RangeOf(GetModuleHandleW(nullptr));
    ResolveCoreRanges();   // before a single hook is in, so no hooked call ever takes the loader lock

    // Direct, compute and copy command lists each have their own vtable in this runtime, pointing
    // at the same functions, measured on 2026-09-13. DirectStorage uploads on copy lists, so all
    // three are hooked.
    const D3D12_COMMAND_LIST_TYPE types[] = { D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_LIST_TYPE_COPY };
    const char* names[] = { "direct", "compute", "copy" };
    for (int i = 0; i < 3; ++i) {
        ID3D12CommandAllocator* allocator = nullptr;
        ID3D12GraphicsCommandList* list = nullptr;
        if (SUCCEEDED(device->CreateCommandAllocator(types[i], IID_PPV_ARGS(&allocator)))
            && SUCCEEDED(device->CreateCommandList(0, types[i], allocator, nullptr, IID_PPV_ARGS(&list)))) {
            void** vt = *(void***)list;
            int patched = PatchSlot(vt, kSlotCopyTextureRegion, (void*)&Hook_CopyTextureRegion, (void**)&g_origCopyTextureRegion)
                        + PatchSlot(vt, kSlotCopyResource, (void*)&Hook_CopyResource, (void**)&g_origCopyResource)
                        + PatchSlot(vt, kSlotCopyTiles, (void*)&Hook_CopyTiles, (void**)&g_origCopyTiles)
                        + PatchSlot(vt, kSlotResolveSubresource, (void*)&Hook_ResolveSubresource, (void**)&g_origResolveSubresource);
            Log("[writes] %s command list vtable %p, %d of 4 copy calls hooked", names[i], (void*)vt, patched);
            list->Close();
        } else {
            Log("[writes] could not create a %s command list to find its vtable, those copies are not watched", names[i]);
        }
        if (list) list->Release();
        if (allocator) allocator->Release();
    }

    device->Release();
    queue->Release();
}

void TextureWritesTick()
{
    if (!g_cfg.streamingTrace) return;

    // On the timeline thread, so the loader lock is nowhere near the command list hook.
    if (!g_dsBase[0].load() || !g_dsBase[1].load()) ResolveCoreRanges();

    static uint64_t lastReport = GetTickCount64();
    uint64_t now = GetTickCount64();
    if (now - lastReport < (uint64_t)g_cfg.statsIntervalS * 1000ull) return;
    lastReport = now;

    AcquireSRWLockShared(&g_lock);
    size_t streamed = g_streamed.size();
    ReleaseSRWLockShared(&g_lock);

    Log("[writes] streamed textures %zu (tiled %llu, created writable by shaders %llu unordered access, %llu render target) | "
        "copies into them by DirectStorage %llu, by the game %llu, by anything else %llu, those last two by call: region %llu, resource %llu, tiles %llu, resolve %llu | "
        "copies into other resources %llu, into a new resource at a streamed texture's old address %llu",
        streamed, (unsigned long long)g_streamedTiled.load(), (unsigned long long)g_streamedUav.load(), (unsigned long long)g_streamedRtv.load(),
        (unsigned long long)g_byDirectStorage.load(), (unsigned long long)g_byGame.load(), (unsigned long long)g_byOther.load(),
        (unsigned long long)g_regionHits.load(), (unsigned long long)g_resourceHits.load(),
        (unsigned long long)g_tilesHits.load(), (unsigned long long)g_resolveHits.load(), (unsigned long long)g_intoOther.load(),
        (unsigned long long)g_reusedAddress.load());
}
