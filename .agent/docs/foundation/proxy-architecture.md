---
name: proxy-architecture
kind: doc
description: what the proxy DLL does, in load order, and where each piece lives in the source
updated: 2026-09-20
links: [DEC-001-dstorage-proxy-as-loader, DEC-015-bundled-directstorage-core-loaded-first, directstorage-streaming, engine-flags, telemetry, responsive-ui]
---

# Proxy architecture

Headers under `include/acevo/`, sources under `src/`, one folder per concern, built into
`dstorage.dll` by `build.ps1`. The game loads it as its DirectStorage runtime (DEC-001).

| Folder | Files | Holds |
| --- | --- | --- |
| `src/` | `dllmain.cpp`, `exports.def`, `version.rc` | attach sequence, the export list, the version resource |
| `core/` | `log`, `config`, `iat` | log file, ini reading into `g_cfg`, import table and vtable patching |
| `dstorage/` | `proxy`, `stats` | the four exports, `FactoryProxy`, `QueueProxy`, process wide request counters |
| `engine/` | `flags`, `process`, `streamer`, `session_leak_fix`, `exceptions` | gflags scan and write, priority class, power throttling, timer resolution, the texture streamer's hooks and fixes, the finished sessions the game keeps (see `session-leak-fix`), the throw log |
| `render/` | `dxgi_hooks`, `frame_stats`, `adapter`, `reflex`, `texture_writes` | factory and swap chain hooks, `Present` timing and hitch logging, the card's memory and the auto sizes, the display owner check, NVIDIA Reflex, and the command list copy tracing (developer only) |
| `telemetry/` | `timeline`, `streaming_trace`, `load_sampler`, `memory_census` | the per second CSV thread and the frame CSV flush, the streaming trace rows, the loading sampler, the memory census (all three developer only) |
| `overlay/` | `overlay` | the package override layer (TODO-007) |
| `ui/` | `responsive_ui`, `restyle_fix`, `menu_refresh_fix`, `style_match_fix`, `child_removal_fix`, `cohtml_hooks`, `ui_probe` | the responsive UI and its parts, the shared Cohtml and UI frame hooks, the developer UI probe (see `responsive-ui`) |

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
   write the flags again (`ApplyFlags("late")`, in case a static initialiser reset one, and that
   includes the auto tile pool once it has been resolved), start the
   timeline thread (`StartTimeline`), get the real factory, apply `SetStagingBufferSize`, return a
   `FactoryProxy`. Just before the late pass, `ResolveAutoSizesFallback` reads the card off a
   factory of its own when step 4 has not happened yet, which covers an exe with no factory import
   to patch and any launch order that puts DirectStorage first. On 0.9.1 step 4 runs about two
   seconds earlier, so it finds the work already done and returns. The pass itself lands 361 ms
   (2026-09-18) or 316 ms (2026-09-20) before the engine sizes its tile pool.
3. Game creates queues: `FactoryProxy::CreateQueue` logs the descriptor, optionally raises the
   capacity, wraps the result in a `QueueProxy` when statistics are on.
4. Game creates its DXGI factory: `HookFactoryVtable` hooks `CreateSwapChainForHwnd` and
   `CreateSwapChain`, and `ResolveAutoSizes` reads the render adapter's memory off that factory
   and writes every ini value set to `auto` (the tile pool flag, the staging buffer size). This
   happens before the game's device and pools exist, which is why the values cannot wait for
   step 2's late pass on a second launch order. The engine sizes its tile pool before it creates
   the swap chain, 7 ms before on 2026-09-18 and 68 ms before on 2026-09-20, so the margin is real
   but it is not fixed and nothing should be moved later on the strength of it. When the swap chain is
   created, `HookSwapChain` runs `reflex::OnSwapChain` and hooks `Present` and `Present1` for the
   frame timing and for Reflex, whichever of the two asked for them. Those slots live in the
   vtable inside dxgi.dll, which the whole process shares, so both hooks fire for every swap
   chain anyone makes and each one counts only the newest that carries a D3D12 device. Reflex
   binds to the D3D12 device behind a swap chain and rebinds when the game builds a new one,
   `CheckAutoSizeAdapter` says so when the adapter the sizes came from is not the one the game
   renders on, `LogDisplayOwner` names the adapter that owns the window's monitor, and
   `TextureWritesOnSwapChain` installs the copy tracing when `streaming_trace` is on.
5. Also at attach, when `acevo_mods/` holds files: `overlay::Install` hooks the file functions of
   every loaded module (`PatchEverywhere`) so the package table read at startup can be rewritten.
6. Also at attach: `InstallResponsiveUi` patches Cohtml's and the exe's UI code in memory and registers
   its listeners, `InstallUiProbe` registers the probe's when it is on, and `InstallCohtmlHooks` then
   patches the exe's import of Cohtml's `Library::Initialize` and wraps the game UI's frame post and end.
   The Cohtml library, system and views are hooked as the game creates them.
7. Also at attach: `InstallStreamerHooks` rewrites the texture streamer's call sites and
   `InstallSessionLeakFix` the one call that makes a local server connection, each only when every byte
   it depends on matches the build it was written for (see `directstorage-streaming` and
   `session-leak-fix`).

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
- `render/frame_stats`: `HookSwapChain` runs `reflex::OnSwapChain` first, then patches `Present`
  and `Present1` if `frame_stats` is on or Reflex actually took, so they do not go into the
  vtable that the whole process shares for a layer the vendor check turned away. Each consumer
  then checks its own setting inside the hook. `OnPresent` ignores any swap chain but the newest
  one carrying a D3D12 device, which is the game's renderer rather than a splash or an overlay,
  then records the time since the previous present, counts hitches
  and buffers per frame samples with the streaming requests since the previous frame.
- `render/dxgi_hooks`: the two factory creation hooks. Each logs the swap chain description, hands
  the swap chain to `HookSwapChain`, and then runs `CheckAutoSizeAdapter`, `LogDisplayOwner` and
  `TextureWritesOnSwapChain`. The legacy `CreateSwapChain` one has never been seen to fire, since
  D3D12 requires the `ForHwnd` path, and it does the same work anyway so that nothing would
  quietly not happen if it ever did.
- `render/reflex`: NVIDIA Reflex on a game that has none. `OnSwapChain` binds to the D3D12 device
  behind a swap chain, checks the adapter's vendor before nvapi is loaded at all, and rebinds when
  the game builds a new device. `OnFrameBegin` sleeps once per present of the swap chain it
  follows, after the real `Present` has returned.
- `render/texture_writes`: with `[developer] streaming_trace=1`, hooks the copy calls on all three
  command list types and counts writes into textures the tile queue streams, split by who made
  them. `TextureWritesOnSwapChain` resolves the DirectStorage cores' module ranges so the hook
  itself never takes the loader lock, and `TextureWritesTick` retries that while either is
  missing and writes the `[writes]` line.
- `render/adapter`: `AutoTilePoolMb` and `AutoStagingMb` hold the size rules by dedicated
  memory, `ResolveAutoSizes` applies them once,
  `ResolveAutoSizesFallback` does the same off its own factory when no game factory arrives,
  `CheckAutoSizeAdapter` checks the adapter they came from against the one the game renders on,
  `LogDisplayOwner` compares the monitor's adapter with the D3D12 device's adapter LUID.
- `telemetry/timeline`: `TimelineThread` wakes every second, refills the hitch log budget and
  ticks the throw log, and while a CSV is on it also resets the counters, queries video memory on
  the discrete adapter (`FindRenderAdapter`) and process CPU time, writes one CSV line and
  flushes the frame buffer.
- `overlay/overlay`: `BuildToc` rewrites the package table in memory, `Hook_ReadFile` serves it,
  `OverlayRedirect` points DirectStorage requests at the loose files (see `content-package`).
- `ui/cohtml_hooks`: listener lists for the Cohtml library, its views and the game UI's frame post and
  end, and the hooks that call them. `ui/responsive_ui` installs the responsive UI's parts, the page fixes
  script and the resource work move, each part in its own file (see `responsive-ui`).
- `engine/streamer`: the texture streamer's hooks, its three fixes and the `[streamer]` line (see
  `directstorage-streaming`). `engine/session_leak_fix` frees the sessions the game keeps (see
  `session-leak-fix`). `engine/exceptions` counts the game's own C++ throws for the throw log.

## What it never does

- Write to game files or the content package. The overlay changes the table only in memory, and code
  patches (the texture streamer, the responsive UI, the session leak fix) change the loaded image only.
- Hook anything on the render thread beyond `Present` and the game UI's frame post and end.
- Run code for a feature the ini disables: each piece checks its flag and returns early.
