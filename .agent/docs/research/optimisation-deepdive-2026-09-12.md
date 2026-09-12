---
name: optimisation-deepdive-2026-09-12
kind: doc
description: eighteen agents across eight angles hunting optimisation outside streaming and VRAM, then every kill re-verified by hand, 24 killed for good and 9 sent back to unresolved of which the DLSS one then closed, the engine is well built and the live leads both sit on one serial chain at the end of a session load, the dynamic track preset and a duplicated tyre build
updated: 2026-09-12
links: [moddability, directstorage-streaming, one-percent-low-hunt-2026-09-05, BUG-012-pit-lane-return-freezes-over-a-second, BUG-009-one-percent-lows-far-below-average, BUG-019-car-physics-rebuilds-every-tyre-model-five-times, TODO-013-faster-session-loads]
---

# Optimisation deep dive

The owner asked what is left outside streaming and video memory, in any shape, not necessarily the
current mod's. Eight angles were surveyed independently, each was then handed to an adversarial
reviewer told to refute it and to default to rejecting when uncertain, then a completeness critic
asked what had been missed and a synthesis ranked what survived. Eighteen agents.

**Seven findings survived, thirty three were killed.** The headline is the negative: this engine is
well built, and most of the obvious places to look are already correct.

The owner then asked for every kill to be re-checked by hand, with the rule that a kill stands only
when it is certain and anything short of that goes back to unresolved. **Nine of the thirty three
came back.** That pass is the last section of this doc and it is the part to read first, because one
of the nine is worth two seconds of every Nürburgring load.

## The negatives, which are the useful part

These cost the most effort and are worth writing down so nobody spends the effort again.

- **The D3D12 submission path is clean.** Two root signatures for the whole material system, GPU
  driven instancing through `ExecuteIndirect` with command signatures, compute shader cluster and
  instance culling with HiZ occlusion, tiled light culling, cached shadow cascades, parallel command
  list recording. There is no barrier churn or descriptor thrash to harvest. This was the angle most
  likely to pay, because the reference machine is GPU bound, and it paid nothing.
- **The shipped geometry is well authored.** LOD chains are real, shadow LODs are set deliberately,
  bounds are tight. A finding that 39 percent of a session's mesh bytes are constant valued UV sets
  and vertex colours reproduced as a byte measurement and was killed on the payoff: nothing showed
  it reaches the frame.
- **Every DLL worth intercepting is already reachable from the DLL the mod ships today.** 48 static
  imports, no delay imports. So a different injection shape buys nothing on its own.
- **Variable rate shading is not a missed feature.** The engine ships it, `disable_vrs` defaults off,
  the card is Tier 2, and of 10,093 materials the 30 that set a rate are almost all particles,
  smoke, spray and rain. Kunos used it where a sane renderer uses it. The only residue is an
  authoring gap, 20 of 43 particle materials carry no rate while 23 siblings do, worth low fractions
  of a percent.
- **The incremental link thunk table is real and irrelevant.** The exe is linked `/INCREMENTAL`, so
  355,594 five byte jump slots sit in the first 1.7 MB of `.text` and 476,036 call sites route
  through them. Measured against the project's own sampler history: 6 of 240 sampled game code
  buckets fall inside the table, about 0.3 percent of render thread CPU. Flattening it would turn
  39.7 MB of clean file backed pages into private dirty commit for nothing.
- **`WinVerifyTrust`, `OptickCore.dll` and `WinPixEventRuntime.dll`** are all dead ends. The first is
  peripheral driver signature checking at input init. The last two ship in the folder with zero
  references anywhere in the exe, which also means any future instrument has to bring its own
  markers.

**Nothing found touches BUG-009**, the render thread handing its main batch to the GPU 3 to 5 ms
late, which remains the open question behind the one percent lows.

## Lead one, closed: the DLSS render preset is hardcoded where no player can reach it

**Closed on the owner's word, 2026-09-12.** The NVIDIA app overrides the DLSS version and the
render preset for this title, so whatever the exe asks for is replaced before it reaches the
runtime. Neither the hardcoded 10 nor anything the mod could force survives that override, which
makes the whole lead unreachable rather than merely unverified. No further DLSS work.

The rest of this section is kept as the record of what was found, not as an open item.

Found by the completeness critic after all eight angles walked past the upscaler.

The exe statically links NVIDIA's NGX helper from `rel_310_5` and ships its own `nvngx_dlss.dll` at
310.5.0.0. One function at `0x141F90430` sets all six `DLSS.Hint.Render.Preset.*` parameters from a
single dword at `[rbx+0x24]`, six identical three instruction sequences. That dword is written by
the caller at `0x141D4542E`:

```
mov   ecx, 0xa
cmp   eax, 1
mov   eax, 0xd
cmovg ecx, eax
mov   dword ptr [r14 + rax + 0x24], ecx
```

So the render preset is **10 in the normal single view case and 13 when a view count is above one**,
and it is not a setting. `VideoSettings.Upscaling` has exactly four fields, `mode`, `fsr3_preset`,
`dlss_preset` (the quality mode, not this) and `dlss_custom_val`. There is no path to the render
preset from the settings file, the UI or the game's own log. The byte pattern
`b9 0a 00 00 00 83 f8 01 b8 0d 00 00 00 0f 4f c8` occurs exactly once in the executable.

**The thing that could make this worth zero, now checked.** The shipped `nvngx_dlss.dll` carries name
strings for `Preset_A` through `Preset_E` and nothing past E, and also carries
`App hint Preset %s is not available, using title default Preset %s`. Confirmed by hand on
2026-09-12 against the shipped 310.5.0.0 DLL: a string sweep, narrow and wide, returns exactly five
preset names, A to E. The game asks for 10 and 13, which are `Preset_J` and `Preset_M` in NGX's
enum, and neither name exists in this runtime.

So the most likely truth is that **the game's own hint is already being refused** and the runtime is
falling back to the title default from NVIDIA's driver profile. That cuts both ways. It means the
hardcoded 10 buys the player nothing today, and it means a preset the mod forced from the A to E
range would be honoured where the game's own is not.

The decisive test is one launch, not a build: turn the NGX log on and read which of
`Using App hint Preset %s` or `App hint Preset %s is not available, using title default Preset %s`
the runtime prints. Until that line is in hand, nothing here is settled.

Shape if it is live: a pattern scan and two single byte writes, exactly the mechanism already used
for 204 engine flags. Size if it is live and 10 really is the transformer model: third party numbers
put transformer against CNN at roughly 7 to 9 percent of frame time on Ampere, which would be the
largest lever found in this project. Unverified either way.

**It is a quality for speed knob, not an engine misbehaviour**, so it collides with
the standing rule that the mod corrects engine misbehaviour and never ships quality settings, so it
is the owner's call. The only part with
a defect flavour is that the preset is unreachable at all, and that the multi view branch asks for
13, a value this DLL does not appear to name.

## Lead two: 86 percent of the dynamic track preset is uncompressed terrain height

The Nürburgring preset is 63,171,718 bytes. **56,060,963 of that is field 4**, a terrain altitude
grid stored as 8,007,680 separate seven byte protobuf submessages with no compression. Field 3, the
rubber state, holds the same 8.0 million values packed into a 16 MB zlib blob that inflates in
45 ms. That asymmetry is the whole finding.

Cross read against `logs/lap7-switch-restart-20260905-2126`: four pit returns at 1530.9, 1548.6,
1560.9 and 1575.8 ms, each opening within 40 ms of the preset log line and closing within 80 ms of
the next physics line, with no streaming behind them.

**This falsifies why BUG-012 was closed.** It was closed on the reasoning that a smaller preset
would change the track's rubber state. The 85.7 percent on offer is terrain altitude, not rubber.

Shape: an offline generator plus the existing override layer, written from the player's own package
into the mod's cache on first sight of a track, exactly the `AddBigScreenFix` pattern. Size: 1.0 to
1.4 s off a 1.53 s freeze that repeats at **every pit return**, and zero effect on frame rate. Per
track: Nürburgring and Fuji about 1.1 s, Spa 0.73, Paul Ricard 0.60, COTA 0.40, Laguna Seca 0.18.

**The "zero at session load" half of this was wrong, corrected 2026-09-12.** It was accepted on the
grounds that the preset parse finishes inside the streaming window that is already running, which is
true of the parse itself and false of what follows it. The parse starts at 10:56:17.121 and the car
physics and car graphics that depend on it run to 10:56:29.563, while streaming ends at
10:56:27.541. That serial chain is the tail of the load and it ends **2.05 s after streaming does**,
so shortening it shortens the load until the 2.05 s is used up. See
[BUG-019](../../bugs/BUG-019-car-physics-rebuilds-every-tyre-model-five-times.md), which shares the
chain and the ceiling.

Risk is real and a crash is the expected failure mode. Nobody has traced whether the engine bounds
checks that vector, and all 44 shipped presets carry it, so there is no precedent. Wet weather is the
likeliest consumer of a terrain altitude grid and needs its own test.

## Free knowledge nobody has collected

- **`log_pso_on_creation`** exists as a flag and has never been run. It would say whether pipeline
  state creation happens during driving, which is exactly the shape of BUG-009 and of the community
  complaint that the first lap at each circuit stutters. `enable_pso_cache` was switched off for the
  headlight bug and never diagnosed: a pipeline cache should be bit identical output, so either it is
  keyed wrongly and is fixable, or the flag does more than its name says.
- **178 of the 216 engine flags have never been mentioned anywhere in this repo.** The ones with a
  bearing on open items: `ai_run_dynamic_track` (default true, and Kunos's own help text says "may be
  a performance issue"), `quick_gamemode_swap` (default true, and TODO-013 is open on faster loads
  without mentioning it), `car_update_complete_max_interval` (the tail latency knob above the two
  budgets the repo already knows), and `no_gi` as a ceiling measurement for what global illumination
  costs.
- **`VCOMP140.DLL`**, the MSVC OpenMP runtime, is imported with three symbols and has one fork site
  at `0x1FF67A2` in what looks like a CPU block compression codec reached through a pointer. MSVC's
  OpenMP pool sizes itself from the processor count and spins, so whenever that region opens it
  stacks threads on top of the engine's own workers. `OMP_WAIT_POLICY=passive` in `DllMain` is one
  line, and if nothing moves the item closes permanently.

## The measurement hazard, which applies to all of it

Every A/B above would be judged on `frame_ms` alone, on a laptop whose clocks slide from 1705 to
1512 MHz as it heats. There is no GPU busy column in `acevo_perf_frames.csv`, and the GPU timestamp
marks were removed with the rest of the latency instrumentation. Any pair has to be run back to back
at the same thermal state from a fixed stationary view, not from a lap, or the noise is larger than
everything here except the track preset.

The instrument that would fix that, and settle BUG-009 and the PSO question at the same time, is a
D3D12 device wrapper that timestamps command list batches and counts `CreateGraphicsPipelineState`
during driving. It ships nothing by itself.

## Hand verification of the kills, 2026-09-12

The owner did not believe the kill verdicts. Three were re-checked first, then all thirty three.
This section is the first pass and the one below it is the complete one.

**Held: the three "duplicate of deleted work" kills.** `git show 30c99dc` removes
`src/render/gpu_timing.cpp`, 169 lines, and reading it back confirms the mechanism the reviewers
claimed: an `ExecuteCommandLists` hook, a timestamp query heap, `GetClockCalibration`, `EndQuery`
and `ResolveQueryData`, reaching the device through `ID3D12CommandQueue::GetDevice`. So the D3D12
timing instrument really was built, used across laps 12 to 17, and deleted on the owner's word.

**Held: the residency priority kill, after two counting mistakes of my own.** The claim was that
the engine already calls `ID3D12Device1::SetResidencyPriority` at vtable offset `0x170`.
Disassembly confirms it exactly. The mapper at `0x141e3e6d0` reads an engine enum and produces
`0x28000000`, `0x50000000`, `0x78000000` and `0xa0010000`, which are `D3D12_RESIDENCY_PRIORITY`
MINIMUM, LOW, NORMAL and HIGH, then `movzx eax, word ptr [r8+4]` and `or eax, r10d` folds in a 16
bit sub priority, and the call at `0x141e3e740` is `call qword ptr [rax + 0x170]` with `edx = 1`,
`r8` pointing at a one entry object array and `r9` at a one entry priority array, which is the
`SetResidencyPriority(NumObjects, ppObjects, pPriorities)` signature exactly.

Counting the vtable naively gives the wrong answer twice, which is worth recording: `d3d12.h`
declares `GetAdapterLuid`, `GetResourceAllocationInfo` and `GetCustomHeapProperties` twice each,
once under `#if !defined(_WIN32)` and once under `#else`, because they return structs by value.
Honouring the preprocessor gives 47 real slots, and slot 46 at offset `0x170` is
`SetResidencyPriority`. So the engine runs a finer grained residency system than the proposal, and
the kill is right.

**Did not hold: the std::mutex and condition variable kill.** This is the only finding in the whole
exercise aimed at BUG-009, and it was killed on the premise that "lap 13's sampler cut was
cumulative from sampler start at 23:00:06, and the session's loading did not finish until 23:00:39,
so its `ZwWaitForAlertByThreadId` figures include loading". Both halves of that are wrong, and the
deleted sampler's own source says so:

- It is not cumulative. `g_spanRead = write;` consumes the frame ring at every report, and the log
  line it prints reads `sampler: last minute`.
- Loading cannot contaminate it in any case. The frame filter is
  `if (s.ms < 100.0f && s.end != s.begin)`, so any frame over 100 ms is discarded before sampling,
  and loading frames are hundreds to thousands of milliseconds.

The second summary, at 23:02:06, therefore covers 23:01:06 to 23:02:06, which is entirely after the
Nürburgring load completed at 23:00:39.618. It is pure driving, and in it the outside game code
excesses in slow frames are:

| function | slow | fast, scaled | excess |
|---|---|---|---|
| cohtml | 833 | 90.3 | +743 |
| **ZwWaitForAlertByThreadId** | **762** | **369.3** | **+393** |
| v8 | 196 | 6.3 | +190 |
| NtFreeVirtualMemory | 62 | 0.4 | +62 |
| renoir | 58 | 13.8 | +44 |

So the generic blocking primitive is the second largest excess in slow driving frames, not the
"roughly 1 percent" and "a tenth of a millisecond inside a 6 ms excess" the reviewer corrected it
to. That number is not reproducible from this log.

What the kill got right and keeps: `ZwWaitForAlertByThreadId` is the primitive every SRW lock,
critical section and condition variable blocks in, so it is not evidence that the C++ `std::mutex`
imports are the thing to hook. The finding should go back to unresolved rather than killed, and the
question it asks, which lock is the render thread waiting on during driving, is still open.

Worth noting separately, because it is the largest number in that table: **cohtml is the biggest
single contributor to slow driving frames**, at more than eight times its fast frame share. That is
the UI, closed permanently by the owner in DEC-010 and BUG-014, so it is recorded and not pursued.

### The two content magnitude corrections, re-derived independently

Both survived the deep dive as KEPT findings but with their sizes cut hard by the reviewers, so
the arithmetic was redone from scratch: every one of the 38,514 cooked `.texture` headers in the
package decoded and paired with its `.texturemips` size from the table. That count matches the
survey's 38,514 exactly.

**The dashboard requantise: the reviewer was right, to within 0.6 percent.**

| | files | bytes | recovery by halving |
|---|---|---|---|
| survey claimed | 69 | 712.0 MB | 356 MB |
| reviewer corrected to | 30 | 519.4 MB | 259.7 MB |
| **re-derived here** | **35** | **522.4 MB** | **261.2 MB** |

The survey's error was assuming every display texture was 16 bit and halving the lot. Of the car
textures with `display` in the path, 233 are already BC7, 28 are `B8G8R8A8UNormSrgb` and 19 are
`R8G8B8A8UNormSrgb`. Only the `R16G16B16A16UNorm` set can be halved. The file count differs from
the reviewer's by five, which is a path filter difference, and the byte total agrees to 0.6
percent.

**Imola: the reviewer was right about scope and too conservative about size.**

The survey's raw numbers check out. Imola's texture budget is 1540.8 MB against its claimed
1541 MB, and `compress=0` covers 689.7 MB against its claimed 673.1 MB. Its worked example
reproduces byte for byte: `content\tracks\imola\textures\top_c_mask` is 4096x4096, 13 mips,
`R8G8B8A8UNormSrgb`, `compress=0`, payload exactly 89,915,392 bytes. There is even a sibling,
`top_c_mask_evo`, cooked at 2048x2048 BC7 with `compress=1`, which is good evidence the
uncompressed one is a defect rather than a decision.

What does not reproduce is the reviewer's "265.1 MB of genuinely safe candidates". Filtering
Imola's uncompressed set to the 8 bit RGBA class that BC can take, at 1 MB or more per file, gives
**41 files and 572.8 MB**, roughly double their figure. Their number could not be reconstructed
from any obvious filter. The remaining 1,329 uncompressed Imola files are float formats totalling
only 115.4 MB and are not safely block compressible, so the split is 43 large 8 bit files carrying
almost all of it.

**But the reviewer's conclusion holds and is stronger than they put it.** Counting uncompressed
8 bit textures of 1 MB or more across every track in the package:

| track | files | bytes |
|---|---|---|
| imola | 41 | 572.8 MB |
| common_assets | 1 | 5.8 MB |
| brands_hatch | 1 | 5.8 MB |
| **everything else, including nurburgring** | **0** | **0.0 MB** |

So this is one track of twenty, it is **zero on the Nürburgring**, and the owner drives the
Nürburgring. Both reviewers also agree on the caveat that matters more than any of the totals:
recovered VRAM is **zero**, because the texture tile pool is a fixed 1024 MB heap, so the saving
is package size and streamed bytes on one track, not headroom.

## Every kill re-verified, 2026-09-12

The rule the owner set: a kill stands only when it is certain, and anything short of certain goes
back to **unresolved**, meaning it may be built and tested before it is judged. All thirty three
were worked through on that rule. **Twenty four hold and nine come back.**

### The instrument that invalidated a whole class of kills

Five verdicts rest on a phrase like "zero samples across 52,460 game code samples spanning every
sampler session this project has run". That census does not exist, and reading the deleted sampler's
own source says why.

`src/telemetry/sampler.cpp` at commit `30c99dc^`:

- **It samples one thread.** `HANDLE thread = g_renderThread;` and nothing else. There is no loop
  over other threads anywhere in it. So the render thread is all it ever saw, and the claim that
  those samples span "the render thread, GameThread, Physics and the resource manager workers" is
  false.
- **It discards loading.** `if (s.ms < 100.0f && s.end != s.begin)` drops every frame over 100 ms
  before anything is tallied, and loading frames are hundreds to thousands of milliseconds.
- **It prints only a top twelve.** `Rank(..., 12, true)` for game code and `10` and `8` for the
  labels outside it. Across every session on disk that comes to **192 printed game code rows
  totalling 25,862 samples**, which is where the reviewers' denominator came from.
- **That top twelve is ranked by extra samples in slow frames**, not by total. A cost that is the
  same in a fast frame and a slow one ranks at zero extra and never appears at all, which is exactly
  the shape of an allocator, a lock or a dynamic cast.

The real census is the per second bucket CSV, which no verdict used. Totalled over driving frames
across the eight sampler sessions, **4,360,295 samples**:

| bucket | samples | share |
|---|---|---|
| game | 2,584,331 | 59.27% |
| system | 579,400 | 13.29% |
| wait | 551,870 | 12.66% |
| driver | 361,747 | 8.30% |
| other | 118,824 | 2.73% |
| d3d12 | 50,169 | 1.15% |
| cohtml | 35,423 | 0.81% |
| **heap** | **28,484** | **0.65%** |
| memcpy | 15,504 | 0.36% |
| lock | 8,281 | 0.19% |
| the rest | under 8,000 each | |

So the reviewers judged against 1 percent of the data, from a list that structurally cannot show a
steady cost, taken from a thread and a phase that three of their targets were never on.

The instrument that **can** answer those questions already exists and none of them used it: the load
sampler of 2026-09-12, which walks two dozen threads round robin at 1 kHz and covers loads. It
prints sixteen game addresses and sixteen outside labels per fifteen second window, 119 distinct
addresses across 107 windows, with a detection floor around 12 samples in a 13,254 sample window,
call it 0.1 percent. Three of the five kills below are re-grounded on it and survive.

### The nine that come back to unresolved

| # | finding | what the kill got wrong |
|---|---|---|
| 1 | The Ferrari 296 GT3 builds each tyre model five times | Killed as "worth zero seconds to a user on this machine today". The profiler timeline says the car chain is the tail of the load and runs 2.05 s past streaming. Now [BUG-019](../../bugs/BUG-019-car-physics-rebuilds-every-tyre-model-five-times.md). |
| 2 | The render thread's waits are `std::mutex` and condition variables | Killed on a sampler window that was claimed to include loading. It does not. Already recorded above. The only finding in the exercise aimed at BUG-009. |
| 3 | ~~The DLSS render preset is hardcoded where no player can reach it~~ | Came back as unresolved, then **closed the same day**: the NVIDIA app overrides the DLSS version and preset for this title, so nothing the exe or the mod asks for survives. Eight remain. |
| 4 | 131 of 132 car light blend masks ship 1 or 2 mips of 12 | The reviewer's own words are "unmeasured and unmeasurable with what the mod records today" and "nobody knows". That is the definition of unresolved. |
| 5 | 39 percent of a session's mesh bytes is one value repeated | The VRAM half is genuinely dead, because COLOR and TEXCOORD0 to 3 are in the vertex layout whatever the file holds. The load time half, 246 MB less to read, decipher and parse, was never measured and is testable with the same override layer BUG-017 already uses. |
| 6 | Thread placement from a flat processor count | The kill refutes the framing, correctly: there is no bad pinning because there is no pinning. It does not touch the proposal, which is to add some. Unmeasured and one lap to test. |
| 7 | OpenMP and the Concurrency chore pool | Killed as inert, while this doc's own "free knowledge" section lists `OMP_WAIT_POLICY=passive` in `DllMain` as one line worth trying. The fork site never appears in a load sampler top sixteen, so it is probably nothing, but probably is not certainly. |
| 8 | Tier 2 VRS with a screen space image | The engine's own image really is gated behind the VR foveated setting, verified. The second route, building an image of our own, is not refuted anywhere, only called unlikely. |
| 9 | ~~Opting into the Agility SDK~~ | Killed as unshippable. Route one does need Windows Developer Mode, true. Route two was dismissed as something "a mod cannot change without rewriting the exe's export directory in memory", which is precisely the class of thing this mod does. **Built, measured six ways and removed the same day**: the exports go in, `D3D12Core.dll` 619 loads, and nothing moves, because the game's own code stops at `ID3D12Device12` and a current Windows already provides that. The kill's reasoning was wrong and its conclusion was right. See [DEC-016](../../decisions/DEC-016-agility-sdk-tried-and-removed.md). |

### The twenty four that hold, and what was actually checked

**Re-grounded on the load sampler, conclusion survives the bad reasoning.**

- **Every allocation funnels through nine UCRT imports.** The heap bucket is 0.65 percent of driving
  samples and 0.6 percent of load samples, with `ntdll!RtlAllocateHeap` inside that at 0.4 percent.
  The finding's own estimate was "a few tenths of a millisecond at best". The number it wanted
  already exists and agrees with it, so building the instrument learns nothing.
- **`__RTDynamicCast` is memoisable with one IAT hook.** It is an export of a system module, so the
  sampler labels it directly when a thread is inside it, and it never appears in any list from
  either sampler. During loads that bounds it below the sixteenth ranked outside function, under
  0.3 percent of all samples on all threads.
- **The pool allocator's two locked read modify writes.** The instructions are where claimed,
  `f0 48 0f c1 10` at `0x14278AF7D` and `0x14278AF9C`. The function never appears in 119 printed
  addresses across 107 load windows, so it is under roughly 0.09 percent of load samples on a phase
  with eleven workers allocating at once, which is the exact case the finding said would be worst.

**Checked against the shipped bytes or files.**

- **`pipeline.library` is never invalidated against the game build.** Read the file: `KSPL`,
  version 2, a length prefixed `NVIDIA GeForce RTX 3060 Laptop GPU`, adapter fields, then `KPSO`
  records each with a 16 byte value and a GUID shaped key. It is content addressed and adapter
  keyed, so a build update misses per entry and recompiles that entry. Deleting the file wholesale
  would throw away the entries that still hit. The kill is right and the proposal is negative.
- **Small props with no distance cull that keep casting shadows.** The headline example reproduces
  exactly: `stone_rock_1_lod0.mesh` has `lodOut` absent, `forceShadowLod` true, `shadowLod` 6 on
  LOD0 to LOD4, and LOD5 with `castShadows` absent, meaning false, at 46 triangles from 11 m. So the
  shadow mechanism really is false on that mesh. **The reviewer over reached, though**: a sweep of
  4,826 track meshes finds **166** that do match the mechanism, last LOD casting shadows with no
  cull distance and no forced shadow LOD, worst at 1,560 triangles from 15 m. The kill still stands,
  for a better reason than the one given: the fix is to write a cull distance, which is adding
  pop in, and pop in is closed in
  [BUG-006](../../bugs/BUG-006-distant-objects-pop-in.md) and again by the standing rule that the
  mod corrects engine misbehaviour rather than trading picture for speed.
- **BUG-016's VRAM growth is one census away from being named.** BUG-016 line 51 does say peak usage
  was 4665 MB of a 5226 MB budget "leaving 561 MB", so that figure is remaining headroom and not
  something a fix recovers. The bug's own cache off session moved the peak to 4590 MB, so 75 MB is
  the measured effect of the leading suspect. The prize really was misread. BUG-016 stays open.
- **The engine already sets residency priority.** Held on the earlier pass, recorded above.
- **The Agility SDK export route.** The exe has 49 exports, all `ffx` and `tm_api`, with neither
  `D3D12SDKVersion` nor `D3D12SDKPath`. Confirms the fact, not the conclusion, hence entry 9 above.

**Killed because the finding proposes nothing, verified as such.**

`XInput empty slot polling` (a negative result, and correct), `duplicated textures and reflection
captures` (a negative result, and correct), `what is clean` (a negative result), `proxying any other
DLL adds nothing` (a negative result), `four named per frame passes` (its own shape field reads
"Nothing to build yet"), and `no link time code generation` (its own shape field reads "Nothing to
build", and a six instruction leaf cannot be inlined into 476,036 call sites from outside). The last
one deserves a line of its own: `0x006AB180`, the `Vec4& operator*=(float)` it names, is the
**single hottest game address in every load the load sampler recorded**, 12,295 samples of the
47,535 printed. Nothing the mod can do reaches it. It is the best thing in this document to hand to
Kunos.

**Killed as duplicates of work that exists, verified in git.**

The three D3D12 instrument findings. `git show 30c99dc` removes `src/render/gpu_timing.cpp`, and
reading it back confirms an `ExecuteCommandLists` hook, a timestamp query heap,
`GetClockCalibration`, `EndQuery` and `ResolveQueryData`. It was built, run across laps 12 to 17 and
deleted on the owner's word. Rebuilding it is available if BUG-009 is ever reopened, and it ships
nothing by itself either way. `Repacking content.kspkg` is TODO-013 read back, down to the same
3.3 GB and the same one second ceiling.

**Killed on evidence in the repo, checked.**

- **A D3D12 level pipeline cache, and the engine's PSO cache as a broken async scheduler.** BUG-002
  line 30 does read "Every slow cluster in the report has `pso 0` or single digits". The load
  sampler puts d3d12 at 0.1 percent and the driver at 1.0 percent of a load window. And BUG-015 is
  a live user visible defect that two Overtake reviewers hit. Nothing to win, a real risk to take.
- **Shader replacement at PSO creation.** Zero until a specific wasteful shader is named, and it
  changes what the game draws on a machine used for online and leaderboard laps.
- **Rewriting the dynamic track preset as a stored deflate stream.** 0.050 s of a 1180 ms freeze,
  reproduced from the file. Superseded by the field 4 finding that survived.
- **The Nordschleife collision surface ships as render geometry.** 31 MB of a 4.7 GB load, on a
  phase that is request bound rather than byte bound. It stays the safest test vehicle if the mesh
  rewriter of entry 5 is ever built.
- **A sixth of the tiled payload is 64 KB tile padding.** 19.6 percent, re-derived independently and
  correct. Not recoverable by anything buildable, because a subresource cannot occupy a part of a
  tile, so removing the padding means removing mip levels.
- **The game already forces 1 ms timer resolution, and the Fanatec SDK runs a 1000 Hz timer.** Both
  facts are true and neither is a lever. The one consequence worth carrying is that the mod's own
  `timerResolutionUs` only does anything below 1 ms, because `timeBeginPeriod(1)` at `0x0280E159`
  runs unconditionally at startup before it.
- **Nothing in the mod hooks D3D12, and Prism proves interception works.** Zero by its own
  admission, and it half spotted a real hazard worth keeping: Prism is a paid `dinput8.dll` proxy
  that already intercepts pipeline state creation in this exe, so anyone running both would stack
  two layers on the same path.

### One thing nobody asked about that the data volunteered

The second and third hottest game addresses in every load window are `0x0280E080` and `0x0280E0C0`,
12,215 samples between them. Disassembled, that is the engine's clock: a `QueryPerformanceCounter`
call, a subtract against a stored base, and a convert and divide to seconds. With
`ntdll!RtlQueryPerformanceCounter` at 2.2 percent of a load window beside it, the engine spends
roughly 3 percent of a load asking what time it is. Not reachable by the mod and not actionable,
but it belongs in the same note to Kunos as the `Vec4` operator.

## Still unchecked

- What the per build cost of a tyre compound actually is. BUG-019 brackets it from log line gaps
  because the lines mark starts and not ends.
- Whether the engine bounds checks the terrain height vector, and whether wet weather consumes it.
- ~~Whether server side content validation reacts to overlay served files.~~ **Answered
  2026-09-12.** The owner drove a Touristenfahrten lap on a public multiplayer server with the
  overlay active and the big screen fix serving a rewritten texture header. The join was clean and
  the game log carries no integrity, checksum or mismatch complaint anywhere. One server, one
  session, so it is not a guarantee for every server, but the blanket offline only caution the
  overlay carried since it shipped no longer has anything behind it.
- What happens in the unattributed 1.34 s startup gap in `logs/loadsampler-20260912-1128`.
- The load sampler has been pointed at the game thirty eight times and never once at the mod's own
  file I/O hooks, which every read in a 64 GB package passes through.
