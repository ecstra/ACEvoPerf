---
name: review-2026-09-sweep-review-render
kind: review
description: the render angle of the full review of main, the auto sizes landing on the wrong card and the settings that silently take other fixes down with them, twelve findings plus forty from the hunters and verifiers across four batches, three of them breaks, merged into 0.4 on 2026-09-20
updated: 2026-09-23
links: [spec-reviews, house-rules-agent, directstorage-streaming, reviews-index, DEC-022-every-card-gets-a-size-and-the-pick-is-checked-after, BUG-034-an-integrated-gpu-with-a-large-uma-carve-out-is-read-as-a-card-that-size]
branch: sweep/review-render
status: closed
---

# Review of the render layer

## Summary

One angle of the full review of main run on 2026-09-20 at high. This angle covers
`src/render/adapter.cpp`, `dxgi_hooks.cpp`, `frame_stats.cpp`, `reflex.cpp` and `texture_writes.cpp`.
Two themes dominate. The automatic memory sizing picks its numbers from the wrong adapter and has no
bracket below 7 GB, and three settings that read as independent in the ini are wired in series, so
turning one off silently removes fixes the player never agreed to lose.

Twelve findings, one breaks, five bug, five debt, one nit. Six more from the hunter on batch 1 and
six from the verifier after it, two of those a breaks that the batch's own fixes either created or
left standing.

Fifty two in all when the ledger closed, twenty five from the hunters and fifteen from the
verifiers across the four batches, three breaks, nineteen bug, eighteen debt and twelve nit. The
status line stayed open for three days after the merge on 2026-09-20 and flipped with the proxy
and core angle's.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the auto sizes land on the card the game actually renders on | closed, runtime confirmed | 2026-09-20 |
| 2 | a setting that is off does not take unrelated fixes with it | closed, runtime confirmed | 2026-09-20 |
| 3 | Reflex survives the session it is installed in | closed, runtime confirmed | 2026-09-20 |
| 4 | the leftovers | closed, runtime confirmed | 2026-09-20 |

## Findings

### F-01: AutoTilePoolMb has no lower bracket, so an integrated GPU is handed the 6 GB card's tile pool
- severity: breaks
- found-by: review
- batch: 1
- status: fixed
- fix: 219ff99 then a0d52da, 2026-09-20, the brackets reach down to 256 MB so every card gets a figure sized for it. The first attempt put a floor at 2 GB and wrote nothing below it, which H-01 showed to be worse than the bug.

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
- status: fixed
- fix: 10c9315, 2026-09-20, `CheckAutoSizeAdapter` compares the adapter the sizes came from against the one the game renders on and names both cards and the ini keys to set by hand. The choice itself cannot be corrected, the engine sized its tile pool 7 ms before the swap chain on 2026-09-18.

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
- status: fixed
- fix: c9b9d13 then 5085190, 2026-09-20, both skip paths say what the run loses, and `ResolveAutoSizesFallback` reads the card off a factory of our own at the late flag pass, 361 ms before the engine sizes its pool.

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
- status: fixed
- fix: batch 1's fallback took the memory sizing out from under it, then 2b5a3aa, 2026-09-20, made the ini say what the setting carries and the log name what it takes down, including a `reflex=1` that cannot work beside it.

`src/render/dxgi_hooks.cpp:61` returns on `!g_cfg.dxgiEnabled` before anything is hooked, and
`ResolveAutoSizes` is called at line 44 inside `HookFactoryVtable`, which only ever runs from the two
factory hooks that line gates. The ini says only "leave at 1, needed to measure frame times".

Failure: a player who turns off frame measuring to shed overhead loses the memory sizing that is the
mod's main fix, with `staging_buffer_mb=auto` and `tile_pool_mb=auto` still sitting in their ini and
the log still promising they will be written once the game creates its factory.

Found independently by four reviewers. Verified by me against the source on 2026-09-20.

Narrowed by batch 1 on 2026-09-20. `ResolveAutoSizesFallback` is called from
`src/dstorage/proxy.cpp`, which does not consult `g_cfg.dxgiEnabled`, so with `enabled=0` the
sizes now resolve at the late flag pass instead, 361 ms before the engine sizes its pool. What is
left of this finding is Reflex and the frame times, and the ini line that still calls the setting
a measuring switch.

### F-05: [dxgi] frame_stats=0 turns off NVIDIA Reflex, which lives in a different ini section and stays at 1
- severity: bug
- found-by: review
- batch: 2
- status: fixed
- fix: 4982e86, 2026-09-20, the swap chain hooks go in for either setting alone and each consumer checks its own, so `frame_stats=0` stops timing frames and nothing else.

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
- status: fixed
- fix: e9f6b4b then 03b4468, 2026-09-20, `g_tried` is gone. A swap chain with no D3D12 device costs nothing and is logged once, and the layer waits for one that has it.

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
- status: fixed
- fix: e9f6b4b then 03b4468, 2026-09-20, the binding is the device itself, so a different one rebinds, resets the counters and lets the old one go at the rebind after it.

`src/render/reflex.cpp:141` keeps the device for the life of the process.

Failure: on a TDR, a driver update or any `DXGI_ERROR_DEVICE_REMOVED` the game builds a new device and
swap chain, `OnSwapChain` returns immediately on `g_tried`, and `NvAPI_D3D_Sleep` keeps being called
with the dead device until the hundred failure counter at line 184 switches the layer off for the rest
of the run. The reference on the removed device is never released, which keeps its heaps alive.

### F-08: Present is hooked in the shared DXGI vtable and OnPresent assumes there is only one swap chain
- severity: debt
- found-by: review
- batch: 3
- status: fixed
- fix: 0e387a0 then 03b4468, 2026-09-20, both hooks compare the presenting swap chain against the one they follow, and both follow the newest one that carries a D3D12 device.

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
- status: fixed
- fix: cd0e7fb, 2026-09-20, `LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_SYSTEM32`, the same call batch 1 gave the dxgi.dll fallback.

`src/render/reflex.cpp:102` calls `LoadLibraryW(L"nvapi64.dll")`, which searches the game folder before
System32. This is the only bare name load in the tree, every other load builds an absolute path from
`GetModuleFileNameW` of our own module. The vendor check at `RenderAdapterIsNvidia` gates it, which is
what keeps this at debt. `LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_SYSTEM32` closes it in one line.

### F-10: FromDirectStorage retakes the loader lock on the render thread for a module that is not loaded
- severity: debt
- found-by: review
- batch: 4
- status: fixed
- fix: dd2e5a1 then 2b12091, 2026-09-20, the ranges are resolved where the hooks are installed and retried from the once a second tick, so the hook itself never takes the loader lock and a core that loads late is still picked up.

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
- status: fixed
- fix: a0bd5e3, 2026-09-20, the legacy body does the same four things its sibling does, reaching the factory through a QueryInterface and the window through `desc->OutputWindow`. It has still never fired.

The format string at `src/render/dxgi_hooks.cpp:28` appears in the DLL binaries and in not one
recorded log line, while `CreateSwapChainForHwnd` has 107 hits across the sessions on disk. The D3D12
game only ever takes the ForHwnd path. Unlike its sibling at lines 21 to 25, its body at line 32 also
skips `LogDisplayOwner` and `TextureWritesOnSwapChain`, so if it ever did fire the display owner check
and the write tracing would quietly not happen. This is also the path F-06 rides in on.

Batch 1 added a third omission on 2026-09-20. `CheckAutoSizeAdapter` was put beside the other two
in the `ForHwnd` hook, so the legacy path skips that as well.

### F-12: kId_Unload is resolved by nobody
- severity: nit
- found-by: review
- batch: 4
- status: fixed
- fix: e4f24f8, 2026-09-20, the constant is gone and a comment says why there is no unload, which is that NVIDIA says not to call it from DllMain and DllMain is the only place the mod could.

`src/render/reflex.cpp:16` defines it and nothing passes it to `query()`. `NvAPI_Initialize` is called
at line 112 and there is no matching unload anywhere, so the constant names a cleanup step the mod
does not perform.

### H-01: standing down under 2 GB shipped a worse configuration than the bug it fixed
- severity: breaks
- found-by: hunter
- batch: 1
- status: fixed
- fix: a0d52da, 2026-09-20, the floor and its early return are gone, the brackets reach down to 256 MB and every card gets a figure.

F-01's first fix returned before `g_cfg.stagingMb` was set and before `ApplyAutoFlags`, on the
reasoning that an integrated GPU's dedicated memory is a carve out and not a budget to size from.
`force_canonical_pool_sizes=true` is written at the early flag pass, three seconds before the card
is known and with no memory gate anywhere, so writing nothing is not a neutral act.

Failure: below the floor the machine got the canonical `texturePoolSize` define, 1433 MB at Low and
6144 at Ultra, in place of the 1024 MB it used to get, and the game's own
`SetStagingBufferSize(1024 MB)` went through uncapped in place of 128 MB. That is BUG-003's root
cause restored, on the machines the fix was written for, and `CHANGELOG.md` had just told those
players it was fixed.

Verified by me on 2026-09-20 against `dist/acevo_perf.ini:38`, the early pass log lines at
`18:14:18.445` and the game's 1024 MB request at `18:14:21.809` in `logs/racefree-20260918/`.

### H-02: 2 GB and 3 GB cards cleared the floor into a bracket worked out for a 4 GB card
- severity: bug
- found-by: hunter
- batch: 1
- status: fixed
- fix: a0d52da, 2026-09-20, a 256 MB bracket under 3 GB, so the step below 4 GB is its own.

The floor admitted anything from 2048 MB up and handed it the 512 MB bracket, whose own comment
says it was worked backwards for a 4 GB card. A 2 GB card would have taken 512 MB of tiles beside
the mod's fixed 1433 MB mesh cap, 1945 MB of a 2048 MB card before a single render target.

### H-03: an integrated GPU with a large UMA carve out is read as a card that size
- severity: bug
- found-by: hunter
- batch: 1
- status: wontfix
- fix: ffe1e80, 2026-09-20, filed as BUG-034 and recorded as an accepted limit in DEC-022.

`DiscreteAdapter` ranks on `DedicatedVideoMemory` alone. An AMD APU set to a fixed UMA size
reports it, commonly 2, 4 or 8 GB, takes the matching bracket, and outranks a smaller discrete
card beside it. `D3D12_FEATURE_DATA_ARCHITECTURE.UMA` would settle it and needs a device, which
does not exist until after the pool is made. Not measured, no such machine is in any session on
disk.

### H-04: DEC-009 still carried the old brackets and a reason this batch disproves
- severity: bug
- found-by: hunter
- batch: 1
- status: fixed
- fix: a0d52da then 13b62c9, 2026-09-20, the false sentence in the streaming doc went with the code, and DEC-022 supersedes DEC-009 rather than editing it.

DEC-009 listed brackets that no longer exist and gave as its reason that "the late flag pass comes
after the pool exists". The session of 2026-09-18 has the late pass at 21.804 and the tile pool at
22.165, so the reason is false by 361 ms and F-03's fix is built on that being false. Separately,
the sentence batch 1 had just added to `directstorage-streaming.md`, that under 2 GB "the game
keeps its own sizes", was untrue for the same reason as H-01.

### H-05: the new warning could not fire in the first case F-03 names, and the fallback gave up there too
- severity: debt
- found-by: hunter
- batch: 1
- status: fixed
- fix: 5085190, 2026-09-20, the skipped hook line says what the run loses, and the fallback loads dxgi.dll from System32 instead of looking it up.

`InstallDxgiHooks` returns before `a` and `b` are computed when dxgi.dll is not loaded at attach,
so the warning below them was unreachable in exactly that case. `ResolveAutoSizesFallback` then
used `GetModuleHandleW`, so a first `DStorageGetFactory` that beat the game's dxgi load would have
logged that the entry point was unavailable and stopped. It runs on the game's own thread, not
under the loader lock, so loading it there is safe.

### H-06: the auto tile pool was the one flag written once
- severity: debt
- found-by: hunter
- batch: 1
- status: fixed
- fix: 1193f48, 2026-09-20, `ApplyFlags` writes the resolved auto value again on any phase after the early one.

`ApplyFlags` skipped `auto` at every phase, so `tile_pool_mb` was written at the auto phase and
never again, while every other flag is written twice. The late pass exists because a static
initialiser can reset one, and it runs 2.3 seconds after the auto value lands and 361 ms before
the engine reads it.

### V-01: an adapter reporting no dedicated memory was never picked, so it fell into H-01's configuration
- severity: breaks
- found-by: verifier
- batch: 1
- status: fixed
- fix: 0f35015, 2026-09-20, the ranking takes the first non software adapter and only then compares, so an adapter reporting zero takes the smallest bracket like any other.

`DiscreteAdapter` started `best` at 0 and skipped on `d.DedicatedVideoMemory <= best`, so an
adapter reporting none could never win. `found` stayed false, `ResolveAutoSizes` returned before
`g_cfg.stagingMb` and before `ApplyAutoFlags`, and the machine got the canonical define plus the
game's uncapped 1024 MB staging request. That is H-01 exactly, reached by a different route, and
plenty of integrated parts report zero dedicated memory with everything in shared.

Pre-existing rather than introduced by the batch, but it is the same failure the batch set out to
close and DEC-022 had just claimed nothing stands down.

### V-02: three log lines promised the game's own values stay, which is false for the tile pool
- severity: bug
- found-by: verifier
- batch: 1
- status: fixed
- fix: 0f35015, 2026-09-20, each line now says that the canonical flag takes the define and names the two keys to set by hand.

Two of the three were written by this batch. With `force_canonical_pool_sizes` on and
`tile_pool_mb` unwritten the game does not keep its own value, it takes the whole
`texturePoolSize` define. It is the sentence H-04 had just deleted from the streaming doc, living
on in the log.

### V-03: the check that the sizes came from the right card cannot fire on the runs the fallback serves
- severity: debt
- found-by: verifier
- batch: 1
- status: wontfix
- fix: 0f35015, 2026-09-20, recorded as the one gap in DEC-022's consequences.

`CheckAutoSizeAdapter` is called from the `CreateSwapChainForHwnd` hook. Three of the four routes
into `ResolveAutoSizesFallback` are exactly the runs where that hook was never installed, so a
machine that took the fallback can never be told it sized from the wrong card. Without the swap
chain there is no device and no LUID, so there is nothing to check against. The fourth route,
DirectStorage simply arriving first with the hooks in place, composes fine.

### V-04: the two skip lines named an incomplete list and overstated what they had diagnosed
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 0f35015, 2026-09-20.

They named frame times, Reflex and the write tracing, and left out the display owner check and the
new adapter check, which are off for the same reason. The warning also said the exe imports
neither entry point, while `PatchIatByAddress` returns 0 on a null target too, so it also prints
when `GetProcAddress` failed or the imports are delay loaded.

### V-05: the changelog line claimed a fixed symptom on hardware nobody has run
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 0f35015, 2026-09-20, moved from Fixed to Changed and reworded to what changed.

"Crashes and missing icons on cards with less than 5 GB" states an outcome. No card under 5 GB
appears in any session on disk, so the outcome is an inference from the mechanism.

### V-07: four statements left over after the second verify pass
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-20, in the commit that closed the batch.

The second pass came back clean on all fifteen claims and named four sentences that were thinner
than the code. The no adapter log line said "set both by hand" while naming one key. The two
fallback lines said the size is left to the canonical flag without the condition, and with that
flag off it falls to the engine's own formula instead. `engine-flags.md` said `tile_pool_mb` is
"always written", which the no adapter case makes false. `directstorage-streaming.md` described
only the integrated case that reports a few hundred MB and not the one that reports none, which
DEC-022 does name. The changelog line was also true of cards under 5 GB but not of an 8 GB UMA
carve out, so it now speaks of cards smaller than 6 GB.

### V-06: three cosmetic effects of the batch, accepted
- severity: nit
- found-by: verifier
- batch: 1
- status: wontfix
- fix:

Kept here so they are not rediscovered. On the fallback path `tile_pool_mb` is written at the auto
phase and again microseconds later at the late pass, four log lines for two storages, idempotent.
`g_autoTilePoolMb` is set before `WriteFlag` reports anything, so on a build where the flag does
not exist "not found in this game build" now logs twice. `lateApplied` in `DStorageGetFactory` is
an unsynchronised read then write, which is older than this batch, and the narrowest interleave it
allows only skips the second write of a value the first write already landed.

### H-07: frame_stats=0 also silences hitch_ms and both developer CSVs, and two log lines say otherwise
- severity: bug
- found-by: hunter
- batch: 2
- status: fixed
- fix: 2026-09-20, in the batch 2 hunter round. The install line says what `frame_stats=0` takes down and the ini note names them.

F-05's shape again, one setting reaching into another section, and the fix's new ini note implied
Reflex was the only casualty. `g_cfg.hitchMs` has exactly one consumer in the tree,
`frame_stats.cpp:53`, which sits below the guard the fix added, so `[log] hitch_ms` produces not one
line with `frame_stats=0`. The frames CSV gets its header and never a row, and the timeline CSV's
frame columns are all zero. Meanwhile `dllmain.cpp:57` and `timeline.cpp:147` both still print
`hitch_ms=33`, so the log positively states a logger that is dead.

Pre-existing rather than introduced, like V-01 was. No session on disk has ever run with
`frame_stats=0`, so it has never been seen.

### H-08: the load order half of the doc still taught the old wiring, 27 lines above the line the fix corrected
- severity: debt
- found-by: hunter
- batch: 2
- status: fixed
- fix: 2026-09-20, in the batch 2 hunter round.

`4982e86` rewrote the `render/frame_stats` entry in `proxy-architecture.md` and left step 4 of the load
order in the same file saying the hooks go in "for frame timing", with no mention of Reflex, although
`reflex::OnSwapChain` fires at exactly that point. Same file, same commit, missed.

### H-09: two findings in the public docs ledger were falsified by this batch and left open
- severity: debt
- found-by: hunter
- batch: 2
- status: fixed
- fix: 2026-09-20, both closed in place with what actually happened.

`sweep/review-public-docs` F-03 and F-04 quote the two ini lines this batch rewrote. F-04 is this
branch's F-05 seen from the docs side. F-03 asks for a comment naming the memory sizing, which batch 1
made untrue when the fallback stopped consulting `dxgiEnabled`. Left open, whoever picks up that branch
would have re-fixed a fixed line and written a sentence that is now wrong.

### H-10: the install line reported frame_stats as if it still decided whether the swap chain is hooked
- severity: debt
- found-by: hunter
- batch: 2
- status: fixed
- fix: 2026-09-20, the line carries both settings, and the config summary carries `[latency]` too.

Three lines below the fix, `DXGI: IAT hooks ... (frame_stats=%d)` was the one place the two settings
could have been shown together. With `frame_stats=0 reflex=1` a reader would have concluded the swap
chain path was dead. The config summary at `dllmain.cpp:57` also never carried `reflex` or
`reflex_boost` at all, so `logs/reflex-C-off-20260912` cannot be read to tell a deliberate `reflex=0`
from a layer that failed on its way in.

### H-11: with frame_stats=0 on a card that is not NVIDIA the present hooks went in for nothing
- severity: debt
- found-by: hunter
- batch: 2
- status: fixed
- fix: 2026-09-20, `reflex::Active()` is asked first, so the hooks go in only if one of the two consumers really wants them.

`reflex=1` ships on, so an AMD or Intel player who sets `frame_stats=0` to shed the hook kept it. These
patch the vtable inside dxgi.dll, which the whole process shares and nothing unhooks, so it is for the
life of the run. `OnSwapChain` runs the vendor check at the same moment, so the order was the only
thing in the way.

### H-12: both swap chain hooks read the caller's descriptor before the runtime validates it
- severity: nit
- found-by: hunter
- batch: 4
- status: fixed
- fix: a0bd5e3, 2026-09-20, both hooks check the descriptor before logging it.

`dxgi_hooks.cpp:19` and `:31` log `desc->Width` before calling the original, so a null descriptor faults
inside our hook instead of returning `E_INVALIDARG`. No real caller passes null, the batch 1 hunter
judged it below the bar and this one filed it. Left for batch 4, which owns the second of the two lines
through F-11.

The verifier's correction, kept because the scoping was wrong and the reason matters: the line that
could actually fault on 0.9.1 is `:19`, the `ForHwnd` one, since F-11 records that the legacy hook has
never fired in any session on disk. So batch 4 carries the live half on the dead half's ticket. It stays
deferred because the two are one change, not because `:19` is harmless.

### V-08: three statements this batch wrote were thinner than the code, two of them copied from batch 1
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-20, in the commit that closed the batch.

The new `frame_stats=0` line said the frames CSV and the timeline CSV "have no switch of their own",
and `[developer] frames` and `[developer] timeline` are exactly that. What is true is that their own
switch is not enough. It also missed the `Present sync interval` line, which goes quiet too. Three log
lines said "the write tracing" is off, while `NoteStreamedResource` and `TextureWritesTick` keep
running and only the copy counters stop, and two of those three lines were shipped in batch 1 and
copied into the third here. And `proxy-architecture.md`'s own `render/frame_stats` entry, plus the
comment in `frame_stats.h`, still said the hooks go in for either setting alone, which H-11's reorder
made false on a card the vendor check turns away.

The last of those is H-08's shape happening again inside the batch that fixed H-08: one half of a file
updated, the other half left. That is the third time on this branch.

### V-09: the frame_stats=0 line cannot fire when the whole section is off
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-20, the `enabled=0` line names them itself.

`InstallDxgiHooks` returns on `!dxgiEnabled` before reaching the `frameStats` check, so a run with
`enabled=0` lost the hitch lines and both CSVs' frame columns with nothing naming them.

### H-13: the frame times latched the first swap chain and never let go, which is F-06 and F-07 put into the sibling file
- severity: bug
- found-by: hunter
- batch: 3
- status: fixed
- fix: 03b4468, 2026-09-20, the frame times follow the newest swap chain that carries a D3D12 device, and say so when they move.

F-08's fix gave Reflex a device keyed binding with a rebind and gave the frame times a first come
latch with neither. With the shipped `frame_stats=1` the latch is taken by any swap chain at all,
so a splash or an overlay making one before the game's would have owned the timing for the run,
and every game present after it would have been discarded. Before the fix those presents were at
least counted, interleaved with the other chain's. After it they were counted nowhere.

The same latch also survived F-07's own scenario. A device reset builds a new swap chain, Reflex
rebinds, and the frame times stay pointed at the address of a destroyed one for the rest of the
run.

### H-14: a second swap chain on the same device left Reflex pacing one that was gone, silently
- severity: bug
- found-by: hunter
- batch: 3
- status: fixed
- fix: 03b4468, 2026-09-20, the same device with a different swap chain follows the new one and logs it.

`OnSwapChain` returned on `device == g_device` before it reached `g_swapChain = swapChain`. A
resolution or window mode change replaces the swap chain without touching the device, so
`g_swapChain` kept the dead one's address, `OnFrameBegin` failed its comparison on every present
and `NvAPI_D3D_Sleep` was never called again. `g_active` stayed true, `Active()` kept returning
true, and the last word in the log was still `[reflex] on`. Before F-08's fix Reflex survived a
swap chain swap, because it paced every present. The fix moved the bug into the silent direction.

### H-15: the rebind let go of the dead device before it stopped pacing
- severity: bug
- found-by: hunter
- batch: 3
- status: fixed
- fix: 03b4468, 2026-09-20, the flag is cleared first and the dead device is held to the next rebind rather than released under a present in flight.

`g_device->Release()` came before `g_active = false`, and between the two a present on another
thread reads a true flag and a pointer whose last reference may have just gone, then hands it to
the display driver. A TDR tearing down the old device is exactly when our reference is likely to
be the last one, and a TDR is the scenario F-07 was written for.

Holding it to the next rebind means at most one dead device is kept rather than one per reset,
which is the part of F-07 that mattered. The flags themselves are atomic now, because the batch's
own premise is that presents and swap chain creation are not on the same thread.

### H-16: the give up flag latched a per device answer for the whole process
- severity: bug
- found-by: hunter
- batch: 3
- status: fixed
- fix: 03b4468, 2026-09-20, the vendor answer is remembered per device, only the nvapi lookup latches for the process.

`RenderAdapterIsNvidia` reads the device's own adapter LUID, so it answers for that device. The
fix latched it as final. On a hybrid machine, which is the machine this mod is developed on, a
D3D12 swap chain on the integrated adapter arriving first would have turned the game's NVIDIA
device away for the rest of the run. That is F-06 reached through the flag that replaced
`g_tried`.

Two smaller ones came with it. `RenderAdapterIsNvidia` returned false with no log at all on a
factory failure and on a LUID that matches nothing, both of which now latched something. And the
one refusal that genuinely is per device, `SetSleepMode`, was the one not remembered, so every
later swap chain re-enumerated every adapter and reprinted the refusal.

### H-17: both halves of the load order doc still taught the wiring the batch replaced, and no changelog line was written
- severity: debt
- found-by: hunter
- batch: 3
- status: fixed
- fix: 03b4468, 2026-09-20.

Step 4 said the hooks go on "its vtable", the swap chain's, and that it is dxgi's and shared by
the process is the single fact F-08 exists to act on. The entry 28 lines below already said the
right thing, put there by batch 2, so the file contradicted itself and the stale half was the one
batch 3 had to correct. Fourth time on this branch that one half of a file moved and the other
did not. Nothing in `.agent/docs/` recorded any of the three fixes, and the three commits carried
no changelog line although two of them are player visible in the same sense batch 2's was.

### H-18: the header's new contract paragraph stated two things the code does not do
- severity: debt
- found-by: hunter
- batch: 3
- status: fixed
- fix: 03b4468, 2026-09-20.

"Called for every swap chain the process makes" is false with `[dxgi] enabled=0`, for a chain from
a factory created before the vtable was patched, and when `HookSwapChain` returns early. "The
device already bound means nothing to do" was H-14 written down as if it were intended.

### H-19: the second swap chain line told the reader the numbers survive when they do not
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: 03b4468, 2026-09-20.

"The numbers below are the one hooked first" read as reassurance in the one case where the whole
measurement had gone dead. V-08's shape again.

### V-10: the changelog line described a symptom that never reached a player
- severity: bug
- found-by: verifier
- batch: 3
- status: fixed
- fix: 2026-09-20, in the commit that closed the batch.

The line claimed Reflex stopped "after a driver reset or a resolution change". The driver reset
half is a real 0.3.2 bug. The resolution change half was created by `0e387a0` and fixed by
`03b4468`, both on this branch and neither released, and `public-docs.md` says work that landed
and came out again before a release gets no entry. Its other half, "when something else on your PC
drew to the screen before the game did", pointed at another application when the trigger has to be
a swap chain made inside the game's own process. Both halves also stated player visible outcomes
for scenarios that appear in no session on disk, which is V-05's shape.

### V-11: the comment credited the store ordering with closing a window the deferred release closes
- severity: bug
- found-by: verifier
- batch: 3
- status: fixed
- fix: 2026-09-20, the comment says which of the two does the work, and the SetSleepMode refusal retires its device the same way.

H-15's fix put `g_active.store(false)` before the device is let go and said that is what keeps a
present safe. It is not. A present that read the flag a moment earlier has already passed its
check, so the ordering narrows the window and never closes it. What closes it is that the device
is retired rather than released, which the same comment's second half said correctly. Getting this
right in the prose matters here, because the wrong half is the one someone would later delete as
redundant.

The refusal path still released immediately, safe only because `g_active` is false throughout it.
It retires now too, so a device that has ever been in `g_device` follows one rule.

### V-12: three log lines asserted things the code has not established
- severity: nit
- found-by: verifier
- batch: 3
- status: fixed
- fix: 2026-09-20.

The frame times retarget line fired with `frame_stats=0`, where nothing is being counted, which is
V-09's shape in the configuration V-09 was found in. It also spoke of what is "in the CSV", which
the shipped `frames=0` never writes. And both retarget lines said the game "replaced" its swap
chain, which is one explanation for a newer chain and not the only one.

### V-13: the load order doc gained the first come rule the hunter had just deleted
- severity: debt
- found-by: verifier
- batch: 3
- status: fixed
- fix: 2026-09-20, both halves of the file say the same thing, and the header no longer claims every swap chain in the process reaches the layer.

H-17's own fix wrote "each one follows only the swap chain it was set up from" into the load order,
which is the first come latch H-13 had removed an hour earlier, while the entry further down said
nothing about which chain is counted. Fifth time on this branch, in the file the finding was about.

The header said "every swap chain made after the factory hooks went in". Only the exe's import
table is patched, so a swap chain from another module's own factory presents through the patched
vtable and never reaches the layer at all.

### H-20: resolving the core ranges once removed the old loop's one virtue
- severity: bug
- found-by: hunter
- batch: 4
- status: fixed
- fix: 2b12091, 2026-09-20, the once a second tick retries while either range is missing, on the timeline thread.

F-10's fix resolved both DirectStorage core ranges at hook install time and never again. If a swap
chain beats the first DirectStorage call, which is the same ordering `ResolveAutoSizesFallback`
exists for, both bases stay zero for the run and every DirectStorage copy falls through to "by
anything else", with a trace row and an exclusive lock per copy on the very hook the fix was
clearing work off. That number being zero is what DEC-017 rests on. The old loop was expensive and
self healing, the fix made it cheap and permanently wrong in that case.

### H-21: the [writes] breakdown was attached to the wrong number
- severity: bug
- found-by: hunter
- batch: 4
- status: fixed
- fix: 2b12091, 2026-09-20.

`region`, `resource`, `tiles` and `resolve` are incremented for game copies and for other copies
alike, and DirectStorage copies return before reaching them, so the four are a breakdown of the
two preceding counters together. The parenthetical sat straight after `by anything else`, reading
as a breakdown of that one. Every captured line has zeros in both fields, so it has never been
visible, and it is the line DEC-017 and `texture-streamer-flip-2026-09-13` both quote.

### H-22: a comment credited an exclusion the code does not perform
- severity: debt
- found-by: hunter
- batch: 4
- status: fixed
- fix: 2b12091, 2026-09-20.

`NoteWrite` said a resource freed and reallocated at the same address is excluded "by the texture
having started streaming before the copy". Nothing compares that stamp. The only filter is the
layout test, which catches a reused address only when the new resource is not tiled, and
`g_reusedAddress` reached 12 in a real session so the case is live. V-11's shape, in a comment
someone would later delete as redundant.

### H-23: the legacy hook could spend the write tracing's install flag and hand it back
- severity: debt
- found-by: hunter
- batch: 4
- status: fixed
- fix: 2b12091, 2026-09-20, the queue check comes before the flag is taken, so a failure never holds it.

Created by F-11's own fix. A legacy D3D11 swap chain on one thread takes `g_hooked`, the game's
D3D12 one on another sees it taken and walks away, the first then fails its QueryInterface and
puts the flag back. Nothing is hooked for the run.

### H-24: proxy-architecture had no entry for two of the five files in its own render folder
- severity: debt
- found-by: hunter
- batch: 4
- status: fixed
- fix: 2b12091, 2026-09-20, `reflex` and `texture_writes` are in the folder table and the pieces list, and `dxgi_hooks` says what both hooks now do.

The folder table listed three of five, the pieces list had entries for three of five, and
`render/dxgi_hooks` still said the hooks "log the swap chain description and hand the swap chain to
`HookSwapChain`", which has been incomplete since batch 1. Sixth time on this branch, in the file
whose own description is where each piece lives in the source, and its `updated` date was already
today.

### H-25: two measurements in the fix's own comment were wrong
- severity: nit
- found-by: hunter
- batch: 4
- status: fixed
- fix: 2b12091, 2026-09-20.

"Thousands of times a second during a load" is about 290 a second in the captures, because
`FromDirectStorage` is reached only after the streamed and tiled checks. "320 to 800 ms before the
first swap chain" is 302 to 846 ms across seven sessions, wrong at both ends and quoting a floor
above the smallest margin recorded. Both inherited from the finding's own wording, which is how a
number nobody re-derives travels.

Also fixed, from the same round: the agent directory ledger's F-09 and F-11 cited line numbers in
these two files that this batch moved. They cite function names now, which is what stops it
happening a third time.

### V-14: H-25's replacement numbers were wrong too, and the ledger edit meant to stop that swapped a stale line for a wrong name
- severity: bug
- found-by: verifier
- batch: 4
- status: fixed
- fix: 2026-09-20, in the commit that closed the batch, measured across all 68 sessions rather than seven.

Three passes over the same two numbers and the first two were both wrong.

The rate: "that hook runs about 290 times a second during a load" put the copy hook's rate at the
rate at which it reaches `FromDirectStorage`. `ai30-A-fix-on` has 690,814 calls into `NoteWrite`
against 19,750 that got as far as the range test, because a copy into anything but a streamed
texture returns earlier. The hook is roughly thirty five times busier than the number claimed, and
H-25's own ledger text had the distinction right before the comment dropped it.

The margin: "302 to 846 ms" came from seven sessions and was then written as "across the sessions
on disk". Measured across all 68 that record both lines it is 285 ms to 2096 ms, median 331, and
the core is never second. Wrong at both ends and the quoted floor again sits above the smallest
margin recorded, which is exactly what H-25 said about the sentence it replaced.

Both now carry the session and the counts they came from, so the next reader can check them
instead of copying them. This one travelled three hops, into the comment, into the ledger and into
the brief the next sub agent was given, and a number that nobody re-derives is how that happens.

Separately, H-24's fix rewrote the agent directory ledger's line number citations into function
names so they would stop rotting, and named `HookVtableSlot`, which is the `core/iat` helper and
does not appear in that file. The local helper is `PatchSlot`.

### V-15: the install flag's one remaining hand back
- severity: nit
- found-by: verifier
- batch: 4
- status: fixed
- fix: 2026-09-20.

H-23 moved the queue check ahead of the flag, and left the `GetDevice` failure below it still
taking the flag and giving it up, which is H-23's shape one step deeper. Unreachable, since
`GetDevice` on a live command queue does not fail, but the whole finding was about that shape. It
holds the flag and logs now, because a second caller let in there could patch slots this one has
already patched.

## Checked and clean

The NVAPI struct layouts and version constants match NVIDIA's headers field for field, with a
`static_assert` behind them.

From batch 1's two verify passes, so none of this is walked again. COM reference counting on every
new path, including each early return. The `IDXGIFactory2` passed where an `IDXGIFactory1` is taken,
which is an upcast and not a QueryInterface. Re entrancy of the fallback's own factory, which
resolves through `GetProcAddress` rather than the exe's import table and never hooks itself. Every
ordering of the game's factory against the first `DStorageGetFactory`, including repeat calls of
each. `wcsncpy_s` into the 128 wide buffer. The vtable slot numbers 10 and 15. The reference
laptop's numbers, 1024 and 128, unchanged by the whole batch in every enumeration order. The five
new log strings against the 4096 byte log buffer and against what the code actually does. Flags that
are not `auto`, untouched by the change in `ApplyFlags`.

From batch 3's hunter. The COM pointer identity the swap chain comparisons rest on: DXGI declares
`IDXGISwapChain1` and `IDXGISwapChain` as a single inheritance chain, so every upcast to
`IUnknown` is a no op and all four pointers to one object carry the same value. The code already
leaned on it before the batch, since `HookSwapChain` takes one vtable off `sc1` and patches slot 8
of `IDXGISwapChain::Present` and slot 22 of `IDXGISwapChain1::Present1` in it, which is only right
if the two share a vtable. Reference counting on every path through the new `OnSwapChain`,
including the rebind and all four refusals. `LOAD_LIBRARY_SEARCH_SYSTEM32` being unavailable,
which would already have broken batch 1's dxgi fallback if it were. And the asymmetric `g_hooked`
reset in `TextureWritesOnSwapChain` that this session raised, which is real but unreachable: the
path that fires on a non D3D12 swap chain is the QueryInterface one, and that one does reset.

One thing nobody can check here. No card other than the 5994 MB RTX 3060 Laptop appears in any
session on disk, so the 256 and 512 brackets, the zero dedicated path, the UMA carve out and the
mismatch warning have never run anywhere.

## The run

Session `logs/render-b1-20260920`, the owner's launch to a loaded track on 2026-09-20. The three
things it had to show, and did:

- `auto sizes: 'NVIDIA GeForce RTX 3060 Laptop GPU' has 5994 MB dedicated -> tile pool 1024 MB,
  staging buffer 128 MB` at 13:08:20.838, the same pick this machine has always had, so nothing in
  the batch moved the reference card.
- `flag tile_pool_mb = 1024 (int32, was 1024) [DeviceAllocator.cpp, late]` at 13:08:22.769, the
  second write H-06 added, which had never existed before.
- `auto sizes: the game renders on 'NVIDIA GeForce RTX 3060 Laptop GPU', the card the sizes were
  picked from` at 13:08:23.174, F-02's check running and agreeing.

The game's own log re-measures both margins the batch is built on. `[Tile Pool] sized to 1024 MB`
at 13:08:23.085, which is 316 ms after the late pass and 68 ms before the swap chain hooks at
13:08:23.153. So the fallback's window holds on a second run and the LUID still arrives after the
pool is made. The margins are not fixed, 7 ms against 68 ms for the swap chain gap across the two
sessions, which is worth remembering before anything else is moved later on the strength of them.

Batch 2 needed a different run, because with the shipped `frame_stats=1` none of its behaviour is
visible. Session `logs/render-b2-20260920`, the same machine with `[dxgi] frame_stats=0` set for
that one launch and put back afterwards. It is the first session on disk ever to run with that
setting, and it is the configuration that used to kill Reflex.

- `[reflex] on: low latency mode (boost off)` at 13:41:05.458, three `[reflex]` lines in all. Before
  this batch there would have been none, no vendor check and nothing saying why. That is F-05.
- `DXGI: [dxgi] frame_stats=0, so no frame is timed ...` at 13:41:02.270, naming what does go with
  it, which is H-07.
- `DXGI: IAT hooks ... (frame_stats=0 reflex=1)` and `config: ... reflex=1 reflexBoost=0`, so the log
  now shows both settings that decide this, which is H-10.
- `DXGI: hooked IDXGISwapChain::Present` at 13:41:05.458, after the three `[reflex]` lines rather
  than before them, which is H-11's reorder.
- Not one `Present sync interval = ` line and not one `[hitch]` line in the whole run, which is what
  the new line claims and the only way to check it.

Batch 3 needed a third run, session `logs/render-b3-20260920`, with the ini back to its defaults
and the owner toggling fullscreen and back mid session. It settles two things the source alone
could not.

- **The swap chain identity holds at runtime, on both hooks.** The whole batch rests on
  `IDXGISwapChain`, `IDXGISwapChain1` and `IUnknown` being one address for one object, which is
  sound from DXGI's single inheritance but had never been executed. `Present sync interval = 0`
  and 62 `[hitch]` lines prove `OnPresent` matched `g_timedChain`, and
  `[reflex] running, 1000 frames paced, 0 refused` with the driver answering `low latency mode ON`
  proves `OnFrameBegin` matched `g_swapChain`. Had either comparison been wrong the counters would
  have stayed at zero and neither line could exist.
- **The engine does not rebuild its swap chain on a window mode change.** The game's own log has
  `[Monitor] update requested ... fullscreen true` at 14:17:55 and `fullscreen false` at 14:18:00,
  and `acevo_perf.log` has exactly one `CreateSwapChainForHwnd` for the run. So it resizes in
  place. H-14's failure, a replaced swap chain on the same device, does not arise through that
  path on 0.9.1, and Reflex kept pacing across both transitions, the thousandth frame landing four
  seconds after the second one.

That makes H-14's fix insurance against a case this engine has not been seen to produce. Worth
saying plainly, and it is not angle one's batch 4 over again: that spent five commits and two
sub agent rounds on a path a single log line then proved dead, this cost one condition and one
log line, and the alternative was Reflex going silent while still reporting itself on.

Batch 4 needed a fourth run, session `logs/render-b4-20260920`, because `streaming_trace` ships at
0 and nothing the batch touches executes without it. Set to 1 for that one launch and put back
afterwards.

- `[writes] ... copies into them by DirectStorage 9295, by the game 0, by anything else 0`. That
  first number being non zero is F-10 and H-20 settled end to end: the core ranges resolved, so
  `FromDirectStorage` answers true and 9,295 copies are attributed to DirectStorage. Had the
  resolution failed, every one of them would have been counted as a write by something else,
  which is the figure DEC-017 rests on being zero. The other two being zero is that figure.
- `4 of 4 copy calls hooked` on the direct, compute and copy vtables, so all twelve slots went in
  with the install flag taken after the queue check rather than before it.
- The reworded line is in the log, `those last two by call: region 0, resource 0, tiles 0,
  resolve 0`, attached to the two counters that actually feed it.
- The two rates in V-14, measured a third time and independently on this run: 1,314 calls a second
  into `NoteWrite` and 346 of those a second reaching `FromDirectStorage`, over the ten seconds
  between the last two `[writes]` lines. The comment says thousands and a few hundred, which is
  what the run shows.
