---
name: review-2026-09-sweep-review-render
kind: review
description: the render angle of the full review of main, the auto sizes landing on the wrong card and the settings that silently take other fixes down with them, twelve findings plus six from the hunter and six from the verifier on batch 1 alone, three of them breaks
updated: 2026-09-20
links: [spec-reviews, house-rules-agent, directstorage-streaming, reviews-index, DEC-022-every-card-gets-a-size-and-the-pick-is-checked-after, BUG-034-an-integrated-gpu-with-a-large-uma-carve-out-is-read-as-a-card-that-size]
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

Twelve findings, one breaks, five bug, five debt, one nit. Six more from the hunter on batch 1 and
six from the verifier after it, two of those a breaks that the batch's own fixes either created or
left standing.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the auto sizes land on the card the game actually renders on | closed, runtime confirmed | 2026-09-20 |
| 2 | a setting that is off does not take unrelated fixes with it | closed, runtime confirmed | 2026-09-20 |
| 3 | Reflex survives the session it is installed in | pending | |
| 4 | the leftovers | pending | |

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

Batch 1 added a third omission on 2026-09-20. `CheckAutoSizeAdapter` was put beside the other two
in the `ForHwnd` hook, so the legacy path skips that as well.

### F-12: kId_Unload is resolved by nobody
- severity: nit
- found-by: review
- batch: 4
- status: open
- fix:

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
- status: deferred
- fix:

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
