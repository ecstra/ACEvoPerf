---
name: optimisation-deepdive-2026-09-12
kind: doc
description: eighteen agents across eight angles hunting optimisation outside streaming and VRAM, 33 findings killed and 7 survived, the engine is well built and the two real leads are an unreachable DLSS render preset and 86 percent of the dynamic track preset being uncompressed terrain height
updated: 2026-09-12
links: [moddability, directstorage-streaming, one-percent-low-hunt-2026-09-05, BUG-012-pit-lane-return-freezes-over-a-second, BUG-009-one-percent-lows-far-below-average, TODO-013-faster-session-loads]
---

# Optimisation deep dive

The owner asked what is left outside streaming and video memory, in any shape, not necessarily the
current mod's. Eight angles were surveyed independently, each was then handed to an adversarial
reviewer told to refute it and to default to rejecting when uncertain, then a completeness critic
asked what had been missed and a synthesis ranked what survived. Eighteen agents.

**Seven findings survived, thirty three were killed.** The headline is the negative: this engine is
well built, and most of the obvious places to look are already correct.

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

## Lead one: the DLSS render preset is hardcoded where no player can reach it

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

**The thing that could make this worth zero.** The shipped `nvngx_dlss.dll` carries name strings for
`Preset_A` through `Preset_E` and nothing past E, and also carries
`App hint Preset %s is not available, using title default Preset %s`. So the runtime may already be
refusing the game's hint and quietly using its own default.

Shape if it is live: a pattern scan and two single byte writes, exactly the mechanism already used
for 204 engine flags. Size if it is live and 10 really is the transformer model: third party numbers
put transformer against CNN at roughly 7 to 9 percent of frame time on Ampere, which would be the
largest lever found in this project. Unverified either way.

**It is a quality for speed knob, not an engine misbehaviour**, so it collides with
[fix-real-bugs-only](../../memory/fix-real-bugs-only.md) and is the owner's call. The only part with
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
1.4 s off a 1.53 s freeze that repeats at **every pit return**, zero effect on frame rate, and zero
at session load because the parse finishes inside the streaming window that is already running. Per
track: Nürburgring and Fuji about 1.1 s, Spa 0.73, Paul Ricard 0.60, COTA 0.40, Laguna Seca 0.18.

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

The owner did not believe the kill verdicts, so three were re-checked by hand. Two held and one
did not.

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

Thirty of the thirty three kills were not re-checked by hand.

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

## Still unchecked

- Whether the DLSS runtime honours preset 10 and 13 or silently refuses them. This decides whether
  lead one is the best item in the project or worth nothing.
- Whether the engine bounds checks the terrain height vector, and whether wet weather consumes it.
- Whether server side content validation reacts to overlay served files. There is no client side
  integrity check in this build, but that does not settle the server side, so everything the overlay
  serves stays offline only until a clean online join is confirmed.
- What happens in the unattributed 1.34 s startup gap in `logs/loadsampler-20260912-1128`.
- The load sampler has been pointed at the game thirty eight times and never once at the mod's own
  file I/O hooks, which every read in a 64 GB package passes through.
