---
name: proxy-architecture
kind: doc
description: what the proxy DLL does, in load order, and where each piece lives in the source
updated: 2026-09-12
links: [DEC-001-dstorage-proxy-as-loader, DEC-015-bundled-directstorage-core-loaded-first, directstorage-streaming, engine-flags, telemetry]
---

# Proxy architecture

Headers under `include/acevo/`, sources under `src/`, one folder per concern, built into
`dstorage.dll` by `build.ps1`. The game loads it as its DirectStorage runtime (DEC-001).

| Folder | Files | Holds |
| --- | --- | --- |
| `src/` | `dllmain.cpp`, `exports.def`, `version.rc` | attach sequence, the export list, the version resource |
| `core/` | `log`, `config`, `iat` | log file, ini reading into `g_cfg`, import table and vtable patching |
| `dstorage/` | `proxy`, `stats` | the four exports, `FactoryProxy`, `QueueProxy`, process wide request counters |
| `engine/` | `flags`, `process` | gflags scan and write, priority class, power throttling, timer resolution |
| `render/` | `dxgi_hooks`, `frame_stats`, `adapter` | factory and swap chain hooks, `Present` timing and hitch logging, the card's memory and the auto sizes, the display owner check |
| `telemetry/` | `timeline` | the per second CSV thread and the frame CSV flush |
| `overlay/` | `overlay` | the package override layer (TODO-007) |

`include/acevo/common.h` holds the Windows, D3D12, DXGI and DirectStorage includes and the version
string. Every header includes it, every source includes its own header first.

## Load order

1. `DllMain` attach (`OnAttach`): read `acevo_perf.ini` from the DLL's folder, open
   `acevo_perf.log`, apply process tweaks (`ApplyProcessTweaks`: priority class, power throttling
   opt out, timer resolution), scan the exe for gflags and write the `[flags]` values
   (`ApplyFlags("early")`), hook the exe's imports of `CreateDXGIFactory1` and
   `CreateDXGIFactory2` (`InstallDxgiHooks`).
2. First `DStorageGetFactory` call from the game: load our own DirectStorage core from
   `acevo_dstoragecore.dll` and resolve its `DStorageGetFactoryCore`,
   `DStorageSetConfigurationCore` and `DStorageCreateCompressionCodecCore`
   (`LoadBundledCore`, DEC-015), falling back to `dstorage_orig.dll` when any of that fails
   (`EnsureReal`), call
   `DStorageSetConfiguration1` with the `[directstorage]` values (`ApplyDStorageConfiguration`),
   log the version of the runtime that really loaded, read off the module itself,
   write the flags again (`ApplyFlags("late")`, in case a static initialiser reset one), start the
   timeline thread (`StartTimeline`), get the real factory, apply `SetStagingBufferSize`, return a
   `FactoryProxy`.
3. Game creates queues: `FactoryProxy::CreateQueue` logs the descriptor, optionally raises the
   capacity, wraps the result in a `QueueProxy` when statistics are on.
4. Game creates its DXGI factory: `HookFactoryVtable` hooks `CreateSwapChainForHwnd` and
   `CreateSwapChain`, and `ResolveAutoSizes` reads the render adapter's memory off that factory
   and writes every ini value set to `auto` (the tile pool flag, the staging buffer size). This
   happens before the game's device and pools exist, which is why the values cannot wait for
   step 2's late pass on a second launch order. When the swap chain is created, `HookSwapChain`
   hooks `Present` and `Present1` on its vtable for frame timing and `LogDisplayOwner` names the
   adapter that owns the window's monitor.
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
- `render/frame_stats`: `OnPresent` records the time since the previous present, counts hitches
  and buffers per frame samples with the streaming requests since the previous frame.
- `render/dxgi_hooks`: the factory creation hooks, they log the swap chain description and hand
  the swap chain to `HookSwapChain`.
- `render/adapter`: `AutoTilePoolMb` and `AutoStagingMb` hold the size rules by dedicated
  memory, `ResolveAutoSizes` applies them once, `LogDisplayOwner` compares the monitor's
  adapter with the D3D12 device's adapter LUID.
- `telemetry/timeline`: `TimelineThread` wakes every second, refills the hitch log budget and
  ticks the throw log, and while a CSV is on it also resets the counters, queries video memory on
  the discrete adapter (`FindRenderAdapter`) and process CPU time, writes one CSV line and
  flushes the frame buffer.
- `overlay/overlay`: `BuildToc` rewrites the package table in memory, `Hook_ReadFile` serves it,
  `OverlayRedirect` points DirectStorage requests at the loose files (see `content-package`).

## What it never does

- Write to game files or the content package. The overlay changes the table only in memory.
- Hook anything on the render thread beyond `Present`.
- Run code for a feature the ini disables: each piece checks its flag and returns early.
