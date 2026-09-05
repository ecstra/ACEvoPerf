---
name: proxy-architecture
kind: doc
description: what the proxy DLL does, in load order, and where each piece lives in the source
updated: 2026-09-05
links: [DEC-001-dstorage-proxy-as-loader, directstorage-streaming, engine-flags, telemetry]
---

# Proxy architecture

One source file, `src/dllmain.cpp`, built into `dstorage.dll` by `build.ps1`. The game loads it
as its DirectStorage runtime (DEC-001).

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

## Pieces

- Logging: `Log`, one file, one critical section, millisecond timestamps.
- Config: `LoadConfig` with `GetPrivateProfile*`, comments stripped after `;` by `TrimComment`.
- Flag scan: `ScanFlags`, two passes over `.text`. Types come from a few well known flag names per
  constructor (the `known` table).
- DirectStorage proxies: `FactoryProxy` implements `IDStorageFactory`, `QueueProxy` implements
  `IDStorageQueue2` and answers `QueryInterface` for the three queue interfaces. Every request is
  counted per destination type into the `g_reqByDest` and `g_bytesByDest` atomics.
- Frame timing: `OnPresent` records the time since the previous present, counts hitches, buffers
  per frame samples.
- Timeline: `TimelineThread` wakes every second, resets the counters, queries video memory on the
  discrete adapter (`FindRenderAdapter`) and process CPU time, writes one CSV line, flushes the
  frame buffer.

## What it never does

- Modify game files or the content package.
- Hook anything on the render thread beyond `Present`.
- Run code for a feature the ini disables: each piece checks its flag and returns early.
