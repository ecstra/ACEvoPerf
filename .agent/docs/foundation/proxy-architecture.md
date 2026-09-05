---
name: proxy-architecture
kind: doc
description: what the proxy DLL does, in load order, and where each piece lives in the source
updated: 2026-09-05
links: [DEC-001-dstorage-proxy-as-loader, directstorage-streaming, engine-flags, telemetry]
---

# Proxy architecture

Headers under `include/acevo/`, sources under `src/`, one folder per concern, built into
`dstorage.dll` by `build.ps1`. The game loads it as its DirectStorage runtime (DEC-001).

| Folder | Files | Holds |
| --- | --- | --- |
| `src/` | `dllmain.cpp`, `exports.def`, `version.rc` | attach sequence, the export list, the version resource |
| `core/` | `log`, `config`, `iat` | log file, ini reading into `g_cfg`, import table and vtable patching |
| `dstorage/` | `proxy`, `stats` | the four exports, `FactoryProxy`, `QueueProxy`, process wide request counters |
| `engine/` | `flags`, `process`, `input_probe`, `device_watch` | gflags scan and write, priority class, power throttling, timer resolution, controller poll timing, device event log |
| `render/` | `dxgi_hooks`, `frame_stats` | factory and swap chain hooks, `Present` timing and hitch logging |
| `telemetry/` | `timeline`, `sampler` | the per second CSV thread and the frame CSV flush, the render thread sampling profiler |
| `overlay/` | `overlay` | the package override layer (TODO-007) |

`include/acevo/common.h` holds the Windows, D3D12, DXGI and DirectStorage includes and the version
string. Every header includes it, every source includes its own header first.

## Load order

1. `DllMain` attach (`OnAttach`): read `acevo_perf.ini` from the DLL's folder, open
   `acevo_perf.log`, apply process tweaks (`ApplyProcessTweaks`: priority class, power throttling
   opt out, timer resolution), scan the exe for gflags and write the `[flags]` values
   (`ApplyFlags("early")`), hook the exe's imports of `CreateDXGIFactory1` and
   `CreateDXGIFactory2` (`InstallDxgiHooks`).
2. First `DStorageGetFactory` call from the game: load `dstorage_orig.dll` (`EnsureReal`), call
   `DStorageSetConfiguration1` with the `[directstorage]` values (`ApplyDStorageConfiguration`),
   write the flags again (`ApplyFlags("late")`, in case a static initialiser reset one), start the
   timeline thread (`StartTimeline`), get the real factory, apply `SetStagingBufferSize`, return a
   `FactoryProxy`.
3. Game creates queues: `FactoryProxy::CreateQueue` logs the descriptor, optionally raises the
   capacity, wraps the result in a `QueueProxy` when statistics are on.
4. Game creates the swap chain: the factory vtable hooks (`HookFactoryVtable`) catch
   `CreateSwapChainForHwnd` and `CreateSwapChain`, then `HookSwapChain` hooks `Present` and
   `Present1` on the swap chain's vtable for frame timing.
5. Also at attach, when `acevo_mods/` holds files: `overlay::Install` hooks the file functions of
   every loaded module (`PatchEverywhere`) so the package table read at startup can be rewritten.

## Pieces

- `core/log`: `Log`, one file, one critical section, millisecond timestamps.
- `core/config`: `LoadConfig` with `GetPrivateProfile*`, comments stripped after `;`.
- `core/iat`: `PatchIatByAddress` for one module, `PatchEverywhere` for all modules except
  kernel32, kernelbase and ntdll (their import tables feed the originals), `HookVtableSlot`.
- `engine/flags`: `ScanFlags`, two passes over `.text`. Types come from a few well known flag names
  per constructor (the `known` table). `ApplyFlags` writes the ini values.
- `dstorage/proxy`: `FactoryProxy` implements `IDStorageFactory`, `QueueProxy` implements
  `IDStorageQueue2` and answers `QueryInterface` for the three queue interfaces.
  `RealDStorageFactory()` hands the unwrapped factory to the overlay.
- `dstorage/stats`: every request is counted per destination type into `g_reqByDest` and
  `g_bytesByDest`, read by the timeline and the hitch logger.
- `render/frame_stats`: `OnPresent` records the time since the previous present, counts hitches,
  buffers per frame samples with the streaming requests since the previous frame, and holds the
  present call when `fps_limit` is set.
- `engine/input_probe` and `engine/device_watch`: controller poll timing into the timeline, and
  a message only window plus an audio endpoint callback that log device changes.
- `render/dxgi_hooks`: the factory creation hooks and the optional latency override. The swap
  chain's `SetMaximumFrameLatency`, `ResizeBuffers` and `SetFullscreenState` are hooked in
  `render/frame_stats` and logged.
- `telemetry/timeline`: `TimelineThread` wakes every second, resets the counters, queries video
  memory on the discrete adapter (`FindRenderAdapter`) and process CPU time, writes one CSV line,
  flushes the frame buffer.
- `telemetry/sampler`: `SamplerThread` suspends the render thread at `sample_us`, classifies its
  instruction pointer by module and by nearest system export, `SamplerOnPresent` cuts the counts
  per frame and keeps the game code histogram of slow frames against the rest (see `telemetry`).
- `overlay/overlay`: `BuildToc` rewrites the package table in memory, `Hook_ReadFile` serves it,
  `OverlayRedirect` points DirectStorage requests at the loose files (see `content-package`).

## What it never does

- Write to game files or the content package. The overlay changes the table only in memory.
- Hook anything on the render thread beyond `Present`.
- Run code for a feature the ini disables: each piece checks its flag and returns early.
