---
title: How the proxy DLL works
updated: 2026-09-05
---

# Architecture

One source file, `src/dllmain.cpp`, built into `dstorage.dll` by `build.ps1`. The game loads it as
its DirectStorage runtime, see the decision record on the proxy approach.

## Load order

1. `DllMain` attach: read `acevo_perf.ini` from the DLL's folder, open `acevo_perf.log`, apply
   process tweaks (priority class, power throttling opt out, timer resolution), scan the game exe
   for gflags and write the values from the `[flags]` section (early pass), hook the exe's
   imports of `CreateDXGIFactory1` and `CreateDXGIFactory2`.
2. First `DStorageGetFactory` call from the game: load `dstorage_orig.dll`, call
   `DStorageSetConfiguration1` with the `[directstorage]` values, write the flags again (late pass,
   in case a static initialiser reset one), start the timeline thread, get the real factory, apply
   `SetStagingBufferSize`, and return a `FactoryProxy` wrapping it.
3. Game creates queues: `FactoryProxy::CreateQueue` logs the descriptor, optionally raises the
   capacity, and wraps the result in a `QueueProxy` when statistics are on.
4. Game creates the swap chain: the factory vtable hooks catch `CreateSwapChainForHwnd` and hook
   `Present` and `Present1` on the swap chain's vtable for frame timing.

## Pieces

- Logging: single file, one critical section, timestamps with milliseconds.
- Config: `GetPrivateProfile*` readers, comments stripped after `;`.
- Flag scan: two passes over `.text`, first for registration sites through the `__FILE__` string
  in `r9`, then for every call to the discovered constructors. Types come from a few well known
  flag names per constructor.
- DirectStorage proxies: `FactoryProxy` implements `IDStorageFactory`, `QueueProxy` implements
  `IDStorageQueue2` and answers `QueryInterface` for the three queue interfaces. Every request is
  counted per destination type into process wide atomics.
- Frame timing: `Present` hooks record the time since the previous present, count hitches, and
  buffer per frame samples.
- Timeline thread: wakes every second, reads and resets the counters, queries video memory on the
  discrete adapter and process CPU time, writes one CSV line, and flushes the frame buffer.

## What it never does

- Modify game files or the content package.
- Hook anything on the render thread beyond `Present`.
- Keep running code when the ini disables a feature: each piece checks its flag and returns early.
