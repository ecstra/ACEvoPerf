---
name: review-2026-09-sweep-review-render
kind: review
description: the render angle of the full review of main, the auto sizes landing on the wrong card and the settings that silently take other fixes down with them, twelve findings, one breaks
updated: 2026-09-20
links: [spec-reviews, house-rules-agent, directstorage-streaming, reviews-index]
branch: sweep/review-render
status: open
---

# Review of the render layer

## Summary

One angle of the full review of main run on 2026-09-20 at high. This angle covers
`src/render/adapter.cpp`, `dxgi_hooks.cpp`, `frame_stats.cpp`, `reflex.cpp` and `texture_writes.cpp`.
Two themes dominate. The automatic memory sizing picks its numbers from the wrong adapter and has no
bracket below 7 GB, and three settings that read as independent in the ini are wired in series, so
turning one off silently removes fixes the player never agreed to lose.

Twelve findings, one breaks, five bug, five debt, one nit.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the auto sizes land on the card the game actually renders on | pending | |
| 2 | a setting that is off does not take unrelated fixes with it | pending | |
| 3 | Reflex survives the session it is installed in | pending | |
| 4 | the leftovers | pending | |

## Findings

### F-01: AutoTilePoolMb has no lower bracket, so an integrated GPU is handed the 6 GB card's tile pool
- severity: breaks
- found-by: review
- batch: 1
- status: open
- fix:

`src/render/adapter.cpp:8` is four brackets that start at `if (vramMb < 7168) return 1024;`. Nothing
below 7 GB is treated differently. `DiscreteAdapter` skips only software adapters, so an Intel or AMD
integrated GPU is accepted on its 128 to 512 MB UMA carve out.

Failure: the shipped ini has `tile_pool_mb=auto`, so this runs for every player.
`AutoTilePoolMb(128)` returns 1024 and `ApplyAutoFlags` writes `tile_pool_mb=1024` into the exe. The
engine reserves a 1 GB tile pool on a GPU with 128 MB dedicated, which is the video memory exhaustion
of BUG-003, BUG-004 and BUG-005 put back on exactly the machines least able to take it. A 4 GB
discrete card gets the same 1024 plus the engine's 1433 MB mesh cap. The staging half has a fallback
inside `SetStagingBufferSize`, the tile pool flag has none.

Verified by me against the source on 2026-09-20.

### F-02: the auto sizes are read from the adapter with the most video memory, never from the adapter the game renders on
- severity: bug
- found-by: review
- batch: 1
- status: open
- fix:

`src/render/adapter.cpp:25` states the assumption in its own comment, "The adapter with the most
dedicated memory is the one the game renders on", and `DiscreteAdapter` ranks on
`DedicatedVideoMemory` with no LUID involved, although `LogDisplayOwner` in the same file already
knows how to get the render adapter's LUID from the device.

Failure: on a hybrid laptop where the game runs on the integrated GPU, by a Windows per app power
setting or a discrete GPU disabled in firmware, the mod reads the idle discrete card and writes
`tile_pool_mb=1536` for a GPU that has none of that memory. `ResolveAutoSizes` is deliberately called
before the device exists, so the LUID is genuinely unavailable at that point and the choice cannot be
corrected there at all. The fix has to either defer the write or re-check once the device appears.

### F-03: an exe with no CreateDXGIFactory import silently loses the whole auto sizing path
- severity: bug
- found-by: review
- batch: 1
- status: open
- fix:

`InstallDxgiHooks` at `src/render/dxgi_hooks.cpp:61` patches the exe's import slots for
`CreateDXGIFactory1` and `CreateDXGIFactory2`. If neither slot exists, because dxgi.dll is not loaded
at attach or because the game resolves the entry point with `GetProcAddress`, both calls return 0.

Failure: `HookFactoryVtable` never runs, so `ResolveAutoSizes` never runs, `g_cfg.stagingMb` stays 0,
both `> 0` gates at `proxy.cpp:377` and `:456` are dead, and `tile_pool_mb=auto` is never written.
That is the staging cap and the pool sizing that closed BUG-003, BUG-004, BUG-005 and BUG-010. The
only signal is one log line printing a pair of zeros, which reads as routine.

### F-04: [dxgi] enabled=0 also turns off the auto tile pool, the auto staging buffer and Reflex
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`src/render/dxgi_hooks.cpp:61` returns on `!g_cfg.dxgiEnabled` before anything is hooked, and
`ResolveAutoSizes` is called at line 44 inside `HookFactoryVtable`, which only ever runs from the two
factory hooks that line gates. The ini says only "leave at 1, needed to measure frame times".

Failure: a player who turns off frame measuring to shed overhead loses the memory sizing that is the
mod's main fix, with `staging_buffer_mb=auto` and `tile_pool_mb=auto` still sitting in their ini and
the log still promising they will be written once the game creates its factory.

Found independently by four reviewers. Verified by me against the source on 2026-09-20.

### F-05: [dxgi] frame_stats=0 turns off NVIDIA Reflex, which lives in a different ini section and stays at 1
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`HookSwapChain` at `src/render/frame_stats.cpp:106` returns on `!g_cfg.frameStats`, and
`reflex::OnSwapChain(sc1)` sits at line 116 below it, the only call to it in the tree. The Present
hooks that drive `reflex::OnFrameBegin` are installed at 111 and 112, also below it.

Failure: a player sets `frame_stats=0` and leaves `[latency] reflex=1`. Reflex never installs, no
vendor check runs, no log line explains it, and README.md:39 still lists Reflex as something they
get.

Found independently by four reviewers. Verified by me against the source on 2026-09-20.

### F-06: g_tried is spent by the first swap chain of any kind, so one non D3D12 swap chain disables Reflex for the session
- severity: bug
- found-by: review
- batch: 3
- status: open
- fix:

`src/render/reflex.cpp:129` sets `g_tried` before anything is validated. The legacy
`IDXGIFactory::CreateSwapChain` path at `dxgi_hooks.cpp:32` can only produce a D3D11 or older swap
chain, because D3D12 requires `CreateSwapChainForHwnd` with a command queue, and it feeds the same
`HookSwapChain`.

Failure: any splash, video surface or injected overlay that creates a swap chain before the game's
D3D12 one makes `GetDevice(ID3D12Device)` fail. The layer logs "the swap chain has no D3D12 device,
layer idle" and the real game swap chain is never looked at. `TextureWritesOnSwapChain` resets its own
flag on the equivalent failure, this one does not.

### F-07: the D3D12 device is captured once and referenced forever, and nothing rebinds after a device reset
- severity: debt
- found-by: review
- batch: 3
- status: open
- fix:

`src/render/reflex.cpp:141` keeps the device for the life of the process.

Failure: on a TDR, a driver update or any `DXGI_ERROR_DEVICE_REMOVED` the game builds a new device and
swap chain, `OnSwapChain` returns immediately on `g_tried`, and `NvAPI_D3D_Sleep` keeps being called
with the dead device until the hundred failure counter at line 184 switches the layer off for the rest
of the run. The reference on the removed device is never released, which keeps its heaps alive.

### F-08: Present is hooked in the shared DXGI vtable and OnPresent assumes there is only one swap chain
- severity: debt
- found-by: review
- batch: 3
- status: open
- fix:

`HookVtableSlot` patches the vtable inside dxgi.dll, so `Hook_Present` fires for any swap chain the
process owns.

Failure: a second swap chain presenting on its own thread races `g_lastPresentQpc`, a plain int64_t,
so the frame times in acevo_perf_frames.csv interleave between two sources. Worse for Reflex,
`OnFrameBegin` runs once per present of any swap chain, so `NvAPI_D3D_Sleep` is called twice per game
frame and paces to the wrong rate.

### F-09: nvapi64.dll is loaded by bare name, so the search order decides which module wins
- severity: debt
- found-by: review
- batch: 3
- status: open
- fix:

`src/render/reflex.cpp:102` calls `LoadLibraryW(L"nvapi64.dll")`, which searches the game folder before
System32. This is the only bare name load in the tree, every other load builds an absolute path from
`GetModuleFileNameW` of our own module. The vendor check at `RenderAdapterIsNvidia` gates it, which is
what keeps this at debt. `LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_SYSTEM32` closes it in one line.

### F-10: FromDirectStorage retakes the loader lock on the render thread for a module that is not loaded
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`src/render/texture_writes.cpp:87` caches the module base, but a base of zero is indistinguishable
from "not looked up yet", so the lookup repeats forever when the module is absent.

Failure: with `bundled_runtime=0` the bundled core is never loaded, so index 0 never resolves and every
hooked `CopyTiles` into a streamed texture, thousands a second during a load, calls
`GetModuleInformation`, which takes the loader lock, from inside a D3D12 command list hook. Gated
behind `streaming_trace=1`, so it bites during a diagnostic run, which is the run whose timings are
being trusted.

### F-11: the legacy CreateSwapChain hook has never fired and skips two things its sibling does
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

The format string at `src/render/dxgi_hooks.cpp:28` appears in the DLL binaries and in not one
recorded log line, while `CreateSwapChainForHwnd` has 107 hits across the sessions on disk. The D3D12
game only ever takes the ForHwnd path. Unlike its sibling at lines 21 to 25, its body at line 32 also
skips `LogDisplayOwner` and `TextureWritesOnSwapChain`, so if it ever did fire the display owner check
and the write tracing would quietly not happen. This is also the path F-06 rides in on.

### F-12: kId_Unload is resolved by nobody
- severity: nit
- found-by: review
- batch: 4
- status: open
- fix:

`src/render/reflex.cpp:16` defines it and nothing passes it to `query()`. `NvAPI_Initialize` is called
at line 112 and there is no matching unload anywhere, so the constant names a cleanup step the mod
does not perform.

## Checked and clean

The NVAPI struct layouts and version constants match NVIDIA's headers field for field, with a
`static_assert` behind them.
