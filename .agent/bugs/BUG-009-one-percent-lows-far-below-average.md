---
name: BUG-009-one-percent-lows-far-below-average
kind: bug
description: the 1 percent low frame rate sits about 20 fps under the displayed average, and about 30 under it on the owner's 5070 desktop with no integrated GPU, so the integrated GPU present path is not the cause, the game updating one UI view per frame in turn is about 30 percent of the gap at the Red Bull Ring GP and the HUD every frame removes it at no cost, this laptop holds no frames for the refresh even at 60 Hz, the rest is spread out renderer code
updated: 2026-09-15
links: [one-percent-lows-2026-09-14, TODO-025-the-ui-view-rotation-test, TODO-026-one-lean-etw-trace-of-the-slow-frames, lap-2026-09-05-nordschleife, one-percent-low-hunt-2026-09-05, BUG-002-fps-drop-entering-new-track-sections, TODO-010-resume-the-one-percent-low-hunt, telemetry, ui-lag-deepdive-2026-09-14]
status: open
severity: bug
area: render
reported: 2026-09-05
parent:
---

## Problem

Owner wording: "The 1% lows are terrible. FPS will be 70-90 and 1% is 20fps lower than the actual
shown fps."

## Evidence

- Lap one of 2026-09-05, driving window: p50 frame time 13.3 ms (75 fps), p99 18.7 ms (53 fps),
  p99.9 50.9 ms. The one percent low is about 20 fps under the median, which is the owner's
  observation in numbers.
- The slow frames are not hitches: nothing over 23 ms while driving. They are a spread of
  16 to 20 ms frames inside a 13 ms stream.
- Periodicity analysis of the driving window (55,758 frames, seconds 150 to 860 of lap one):
  mean 12.7 ms, p99 16.4 ms (61 fps), 0.9 percent of frames over 16.6 ms, almost all of them
  isolated single frames (336 runs of length 1, only 5 runs longer than 3).
  Autocorrelation of frame time: lag 1 is slightly negative (minus 0.07) while lag 2 is plus 0.50,
  lag 4 plus 0.38, lag 6 plus 0.44 and every longer lag plus 0.2 to 0.3. So two things overlap:
  slow neighbourhoods (heavier sections, the same GPU story as BUG-002) and a strong every other
  frame alternation, a long frame followed by a short one.
- Streaming is not involved: seconds with tile requests and seconds without have the same
  spread (standard deviation 1.29 ms, worst frame 16.3 ms in both groups), same for GPU uploads.
- Alternating frame cost matches the engine's time sliced work: car reflection cubemaps rendered a
  few faces per frame (`facesPerFrame` in the exe, `CarReflectionQuality_High` in the owner's
  settings), clouds spread over 16 frames (`renderingTimeslicedOverFramesNumber: 16`), GI probes
  at 16 per frame (`gibake_probes_per_frame`), plus the present path through the integrated GPU
  in windowed mode. (Corrected 2026-09-14. The phase locked cycles are the game updating one UI view
  per frame in rotation, and lap 19 with reflections Low and the mirror off still shows them.)

- Lap three of 2026-09-05 (car reflections Medium, `gibake_probes_per_frame=8`, windowed):
  first stint mean 11.1 ms (90 fps), p99 14.7 ms (68 fps), lag 2 autocorrelation down from
  +0.50 to +0.34 and lag 1 at minus 0.14. The alternation weakened and p99 improved by 1.7 ms,
  the gap to the average is still about 22 fps. The owner reports fps now reaching 100 and the
  gap unchanged in feel. Fullscreen made no difference and was reverted. Lower GI probes made
  shadows flicker and were reverted.

- Lap four of 2026-09-05 (`max_frame_latency=1`, fixed 1024 MB pool, texture quality Ultra,
  car reflections Medium): stint one mean 12.2 ms (82 fps), p99 16.4 ms (61 fps), lag 1
  autocorrelation +0.03 and lag 2 +0.22. The alternation is gone, what remains is the slow
  neighbourhood spread. Owner: "Pacing improved".

- Lap five of 2026-09-05 (first run after the source split, same ini apart from a dropped crash
  dump flag). Owner: "The 1% fix came undone? its back to the old 1% now". The report, windowed to
  the three stints (`telemetry_report.py --from --to`, which now prints the 1 percent low as the
  mean of the slowest frames):

  | stint | avg fps | 1% low | p99 | GPU clock | thermal throttle |
  | --- | --- | --- | --- | --- | --- |
  | lap four | 82.5 | 46.2 | 16.1 ms | 1601 MHz | 91 % of seconds |
  | lap five, stint 1 (pause and settings opened) | 75.9 | 9.6 | 32.3 ms | 1706 MHz | 62 % |
  | lap five, stint 2 | 75.4 | 47.1 | 17.3 ms | 1554 MHz | 100 % |
  | lap five, stint 3 | 76.4 | 34.6 | 18.2 ms | 1573 MHz | 99 % |

  Stint 2 has the same 1 percent low as lap four, the averages are 7 fps lower with the GPU
  clock 50 MHz lower and throttling the whole time (15 minutes into the session). Stint 1 is
  ruined by menu use: every pause and settings open costs 100 to 300 ms of stall, and the
  settings page pushed VRAM to 5337 MB, over the 5226 MB budget. A build and
  a 1.2 GB package extraction also ran on the machine during stint 2. So nothing in the mod
  changed the frame distribution, the run was contaminated and needs a clean repeat.

- Lap six of 2026-09-05 (clean repeat, machine idle, same build): the owner reads 84 fps with a
  72 fps 1 percent low on the in game counter before switching windows, and 86 with 55 after
  coming back. The frames CSV, windowed:

  | window | avg fps | 1% low | p99 | streaming in the window | GPU clock |
  | --- | --- | --- | --- | --- | --- |
  | 19:54:13 to 19:54:58, before the switch | 98.4 | 77.2 | 12.3 ms | 160 MB tiles, 0 MB uploads | 1583 MHz |
  | 19:55:14 to 19:55:25, the return | 82.0 | 12.1 | 39.3 ms | resume reloads the HUD document | |
  | 19:55:25 to 19:56:25, after the return | 80.5 | 53.2 | 16.9 ms | 819 MB tiles, 669 MB uploads | 1520 MHz |

  GPU utilisation is 98 to 99 percent in both driving windows and thermal throttling is active
  in both. The return costs one stall while the HUD comes back (worst frame 131 ms). After that
  the car is in a section that streams 819 MB of tiles and uploads 669 MB of meshes and
  textures in a minute, with the GPU clock 60 MHz lower, and that is where the 1 percent low
  sits at 53. The window switch itself leaves the pacing intact, the section does the rest. So
  the owner's reading is the reload stall plus BUG-002, not a new defect.

- Correction, 2026-09-05 evening: the latency cap credited above never took effect. The game
  calls `SetMaximumFrameLatency(2)` right after creating its swap chain, after the proxy's call,
  so every lap ran at the game's own latency of 2. The first session with 1 truly enforced ran at
  43 to 45 fps instead of 80 or more (DEC-008). Whatever removed the alternation on lap four was
  not the cap, the fixed pools landed in the same session and are the remaining candidate.

- Clean driving, 21:36:29 to 21:37:50 (game's own latency of 2, GPU at 99 percent): average
  89 fps, 1 percent low 48.7 fps, median frame 11.0 ms, p99 15.4 ms. The slowest 1 percent are
  75 frames in 68 separate runs, 62 of them single frames, one every 0.45 s on median. Lag 1
  autocorrelation is gone (minus 0.06), lag 2 still +0.32. So the gap is isolated slow frames at
  a steady rhythm plus a little alternation, not sections and not the menu.

- 21:49 session, the per frame streaming columns in place, clean minute from the pit exit:
  median 12.0 ms, 83 fps, p99 15.6 ms. Frames over 1.3 times the median: 34 of 3833, over 1.5
  times: 3, over 2 times: none, and all the time above the median in those frames is 0.3 percent
  of the window. So in clean driving there is no stall pattern left to remove, the 1 percent
  low is the width of the distribution itself. The same ratio holds in every clean window
  measured today (p99 over median 1.21 to 1.40 with the GPU at 97 to 100 percent and 1520 to
  1583 MHz against a 1987 MHz peak, 87 degrees, power capped near 90 W). Of the slowest 1
  percent, 25 to 40 percent fall on a frame where the engine enqueued texture tiles, against 2
  percent of all frames, so streaming decisions are one of the costs inside the spread, the rest
  is the view itself. The owner's settings: clouds Ultra, volumetrics Ultra, grass Ultra, motion
  blur Ultra, GI update High, DLSS Ultra Quality.

- Three attribution runs, 2026-09-05 22:00 to 22:08, one restart each, the first 45 s from the
  pit exit and the 45 s after on the same stretch (settings restored from the backup afterwards,
  verified byte for byte):

  | run | avg fps | 1% low | slowest 1% mean | median | p99 / median | GPU clock |
  | --- | --- | --- | --- | --- | --- | --- |
  | baseline | 83 | 61 | 16.4 ms | 12.0 ms | 1.29 | 1584 MHz |
  | `no_gi=true` | 92 / 81 | 64 / 61 | 15.6 / 16.5 ms | 10.9 / 12.5 ms | 1.30 / 1.26 | 1807 / 1646 |
  | `disable_dynamic_track=true` | 88 / 78 | 60 / 59 | 16.6 / 16.9 ms | 11.5 / 13.1 ms | 1.32 / 1.24 | 1721 / 1585 |
  | `gpu-relief` profile | 94 / 84 | 64 / 61 | 15.7 / 16.4 ms | 10.5 / 12.0 ms | 1.41 / 1.30 | 1776 / 1613 |

  None of the three narrows the spread. The higher averages in every first window are the GPU
  clock after a restart (cooled GPU), gone in the second window. The slowest 1 percent sit at
  15.6 to 16.9 ms in every run while the median moves with the clock, so the slow frames have a
  cost that does not scale with GPU load. Not a 60 Hz wall: both displays run at 165 and 300 Hz
  and the histogram is a smooth tail from the median with no peak near 16.7 ms.

- Present split, 22:10 session: of the 42 slowest frames of a clean minute none spent more than
  half its time inside the Present call. The slow frames are not the swap chain queue.

- Render thread sampling, 22:26 session (`[profile] sampler=1`, one sample of the render
  thread's instruction pointer every 250 us, sorted by module and by the nearest export of the
  system DLLs). Two clean 45 s windows on the same stretch, shares of the slowest 1 percent
  against the median half:

  | bucket | window 1 slowest 1% | window 1 median half | window 2 slowest 1% | window 2 median half |
  | --- | --- | --- | --- | --- |
  | game code | 59.4 % | 68.9 % | 49.7 % | 65.0 % |
  | kernel waits | 15.8 % | 11.2 % | 35.1 % | 15.9 % |
  | driver | 8.0 % | | | |
  | v8 (UI script) | 4.8 % | 0 % | | |
  | cohtml | under 1 % | | | |

  Samples per frame: 65.6 in the slowest 1 percent against 39.9 in the median half. The extra
  samples of a slow frame are game code (+11.5, about 2.9 ms), kernel waits (+5.9 in window 1,
  +16.9 in window 2), v8 (+3.2, so the UI's script runs on the render thread in slow frames
  only) and the driver (+2.0).

- Where the extra game code is (the exe read statically at the sampled addresses, calls in this
  build go through jump thunks, callers resolved through them): a third of the slow frames' game
  samples sit in the engine's fiber job scheduler (the functions that log "Ran out of fibers" and
  "Ran out of jobs"). The hottest address, rva `0x27ADC80`, is a test and set spin lock
  (`xchg` on the int at offset 0x20 of the scheduler, `pause` loop), 882 samples in slow frames
  against 233 expected from the fast frames. Its unlock at `0x27AF740` has 544 against 146. The
  render thread reaches them from `0x27AF7A0`, a wait on a job counter that runs jobs itself
  while the counter is above zero, called from the renderer's frame function (the one that names
  `TextureFeedbackPass`, `ZPrepass` and `Velocity`) and from the game thread. So in a slow frame
  the render thread spends about 2 ms more spinning for jobs that have not finished, plus the
  extra kernel waits. Work that appears only in slow frames: a sort inside the physics world's
  actor add and remove path (`0x27482D0`, 87 samples against 0), a float3 copy loop
  (`0xB26760`, 69 against 0), and a small parameter apply helper called from 89 places
  (`0x2897A80`, 104 against 1). Each is under a quarter of a millisecond per slow frame.

- The spin lock plus the kernel waits make the slow frames a waiting problem: the render thread
  is idle for most of its extra time, either helping the scheduler or blocked in the kernel. What
  it waits for is not in the samples. The 22:26 build timed only waits on the swap chain's frame
  latency object from the exe's own import table, and that column stayed at zero for all 10,940
  frames, so the game does not wait on that object through the exe's imports.

- Laps 12 to 19, 2026-09-05 evening to 2026-09-06 morning, one instrument added per lap, each
  on the same stretch from the pit exit (the full numbers are in `one-percent-low-hunt-2026-09-05`):

  | lap | what changed | avg fps | 1% low | slowest 1% | what it showed |
  | --- | --- | --- | --- | --- | --- |
  | 12 | every wait of the render thread timed | 89.5 | 57.5 | 17.4 ms | no fence waits, other waits 0.6 ms, present 2.0 ms |
  | 14 | sampler cut per minute, slowest 1% against the median half | 84.3 | 60.4 | 16.6 ms | game code flat, the extra is `NtWaitForSingleObject` under the driver |
  | 15 | tile mappings and submits timed | 84.9 | 59.3 | 16.9 ms | mappings in 9.8 % of the slowest frames against 1.7 %, 0.08 ms each |
  | 16 | GPU timestamps per batch | 82.7 | 57.0 | 17.5 ms | GPU busy 13.1 against 10.3 ms, GPU span 16.6 against 10.9 |
  | 17 | frame latency 3 | 84.9 | 61.4 | 16.3 ms | GPU idle 4.2 ms before the main batch against 0.8, the batch starts 0.16 ms after its submit |
  | 18 | render thread core speed, sampler off | 84.6 | 59.3 | 16.9 ms | core 5 % slower in the slowest frames, CPU at 118 to 126 % of nominal |
  | 19 | reflections Low, shadows Low, LOD Medium, vehicle LOD Medium, mirror off, grass High | 96.3 | 53.9 | 18.5 ms | GPU work flat (9.9 against 9.1 ms), the gap before the main batch is the whole excess |

- The corrected reading of the 22:26 sampler lists: the scheduler spin lock at the top of every
  cumulative list was the loading phase. In a driving minute cut by the report's own rule the
  game code list is flat, the top entry has 19 samples against 11 expected.
- What the render thread's extra time is, in the slowest 1 percent of a driving minute: about
  3 ms of game code spread over the renderer, no hot spot, plus 0.9 to 4 ms in the present path
  through the integrated GPU (`dxgi > d3d11 > atidxx64` on the stack, both displays are outputs
  of the AMD adapter in the game's own log), plus 0.3 to 1.2 ms of HUD script (v8) that runs on
  the render thread only in those frames, plus 0.6 ms of driver.
- The frame interval histogram is one smooth hump from 9 to 15 ms, no peaks at multiples of
  3.33 or 6.06 ms, so the compositor is not pacing the game. Vsync is off in every lap.

## Fix

Absent, and parked on 2026-09-06 after lap 19 at the owner's call. Ruled out with measurements:
the swap chain queue and Present, D3D12 fences, DirectStorage GPU decompression (the game
streams raw data), the refresh rate, the GPU clock and temperature, the CPU clock and the render
thread's core, kernel wait handles, the game's own hot spots, tile mappings, GI, the dynamic
track, the GPU heavy settings and the CPU heavy draw settings, frame latency 1 (halves the
frame rate) and 3 (a few fps, inside run to run noise), the sampler's own load. What is left is
a render thread that hands its main command list to the GPU 3 to 5 ms late in heavy views for
reasons spread across the renderer, a present path through the integrated GPU, and HUD script
on the render thread. The leads and the instruments to bring back are in TODO-010. All of the
diagnostics were removed from the mod on 2026-09-06.

One lead added on 2026-09-12. The deep dive of that day turned up exactly one finding aimed at
this bug, a hook on the exe's `_Mtx_lock`, `_Cnd_wait` and `_Cnd_broadcast` imports to name the
lock the render thread waits on. It was killed by its reviewer and the kill did not survive
re-verification, so it is back to unresolved. Read in the second sampler summary of lap 14,
which covers pure driving with no loading in it, `ZwWaitForAlertByThreadId` is the second
largest excess in slow frames at 762 against 369 scaled, behind cohtml. The question of which
lock that is remains open. See
[optimisation-deepdive-2026-09-12](../docs/research/optimisation-deepdive-2026-09-12.md).

## Read again from the runs of 2026-09-13

Picked back up on the owner's list, "Frametime, Memory Creep (leak), 1% all still remain". No new
lap, the frames of that day's runs read again. They are the first with the GPU clock flat, the owner
had undervolted the card, so the spread is not the throttle's. Windows as in
[frame-time-mod-against-passive-2026-09-13](../docs/research/frame-time-mod-against-passive-2026-09-13.md),
100 s parked at the Nürburgring GP pit exit.

| run | frame to next frame | median | p99 | p99 over median | Reflex, priorities |
|---|---|---|---|---|---|
| N, mod passive | 2.56 ms | 10.34 ms | 13.51 ms | 1.31 | off |
| M, mod on | 1.53 ms | 10.66 ms | 13.41 ms | 1.26 | on |
| P1, mod on with the engine's budgets | 1.93 ms | 10.33 ms | 13.38 ms | 1.30 | on |
| R, release 0.3.1 | 1.61 ms | 10.80 ms | 13.55 ms | 1.25 | on |

- **Without the mod the frames alternate.** Run N goes 8.3, 12.4, 8.2, 12.2, 8.7, 12.1 ms, two humps
  in its histogram, and its frame time correlates minus 0.88 with the next frame. That is the every
  other frame alternation of lap one on 2026-09-05, back with the mod passive.
- **The mod already evens it out.** Every run with the mod on has one hump around 10 to 10.5 ms and
  a frame to frame swing of 1.5 to 1.9 ms against 2.6. Reflex is the likeliest part, it is the one
  change to how frames queue, but the priorities and timer changed at the same time and were not
  split.
- **What is left is width.** With the mod on, p99 is 1.25 to 1.30 times the median, one hump, no
  second mode. An overlay that shows p99 as its 1 percent low reads 74.6 fps against a 92.9 average
  in run M, the 18 fps gap the owner reports.
- **The texture streamer's pass adds a little.** On ten Red Bull Ring laps with the streaming trace
  (`logs/streamer-boot1-1124`, 503 passes, reload fix off) 26.6 percent of the slowest 1 percent fall
  in the first tenth of a second after a pass against 10.2 percent of all frames, and the frame
  presented within 20 ms of a pass averages 0.51 ms longer than the rest. Parked with the reload fix
  on (`logs/streamer-boot2-fix-1141`, 46 passes) no excess shows. So loads the pass starts cost about
  half a millisecond on the next frame while driving, which moves frames near p99 across it, and it
  is not the bulk of the tail.
- **Both monitors are still outputs of the AMD integrated GPU** in every run of the day, the game's
  own `[Monitor]` lines and the mod's `[display]` warning, so every frame is still copied across
  adapters. That path cannot be tested on the reference machine, owner wording: "THERE IS NO MUX on
  this laptop and I do not have any cable that directly connects the GPU to the monitor (Type c to
  hdmi, i do not have. I have connected via hdmi to hdmi)." See
  [reference-machine-has-no-direct-gpu-display](../memory/reference-machine-has-no-direct-gpu-display.md).

Parked again on 2026-09-13. The mod already evens the pacing, what is left is the width of the
engine's own frame times on a laptop that presents through its integrated GPU, and the one lead
that could move it needs hardware the reference machine does not have.

## The deep dive, 2026-09-14

Five angles from the exe, the frames CSVs and the instrumented laps, no new run. The full record is
[one-percent-lows-2026-09-14](../docs/research/one-percent-lows-2026-09-14.md).

- **What the slowest frames are.** The present path first, `Present` returns a nearly fixed 2.5 to
  4.4 ms after the GPU finished the previous frame and carries 50 to 59 percent of the slow frame
  excess. Then 1.2 to 2.8 ms of spread out renderer code. Then periodic pieces, the largest a ripple
  from the game advancing and painting one Cohtml view per frame in rotation over the HUD and the car's
  dashboard displays, a 3 frame cycle for two display cars and a 2 frame one for the Mazda.
- **Not a lock, not a job.** Lock waits add 0.01 to 0.22 ms per slow frame and the job scheduler about
  0.06 ms. The 2026-09-12 table behind that lead included the session load.
- **The UI's own processor time does not set the width**, taking every UI sample out of slow frames moves
  p99 over median by at most 0.006. The rotation reaches frame time through the GPU and the next present.
- **Reflex is what evens the long short alternation**, the split is on disk in the Reflex runs of
  2026-09-12.
- **No processor shortage.** The game uses about 4 of 16 logical processors, and lap 18's slower core was
  the CPU clock in one stretch. The thread and core levers are killed as width fixes.

No fix is shown to narrow the width on the Nürburgring protocol. Two runs with no build decide the next
step, the view rotation test ([TODO-025](../todos/TODO-025-the-ui-view-rotation-test.md)) and one lean
ETW trace started from the owner's elevated prompt
([TODO-026](../todos/TODO-026-one-lean-etw-trace-of-the-slow-frames.md)).

Corrections to this record, each with its evidence in the research doc.

- The alternating frame cost is not reflection cubemaps, clouds or GI probes, see the note above.
- `Present` was not ruled out by "no slow frame spent over half its time inside it". It carries most of
  the excess through its coupling to the previous frame's GPU end.
- The game does wait on its frame latency object, once a frame at `0x1CE3AA6`. The 22:26 build compared
  handle values and each `GetFrameLatencyWaitableObject` call returns its own handle.
- Lap 18's "core 5 percent slower in the slowest frames" follows the CPU clock in one streaming stretch.
- "About 3 ms of game code" is 1.2 to 2.8 ms on laps without GPU marks, and "0.3 to 1.2 ms of HUD script
  only in slow frames" is V8 bursts after a load that do not set the width.
- The lap 14 summary with cohtml first and `ZwWaitForAlertByThreadId` at 762 against 369 is lap 13's
  cumulative build and mostly the load.
- The mod evening the alternation is Reflex, damped rather than gone (M parked frame to next frame
  correlation minus 0.67).

## A desktop with the same gap, 2026-09-15

In the owner's words, "I played the same game on another PC (5070 PC, no iGPU, striaght GPU display.
using nvidia surround display for triple monitor). And it had the same bug. The game was running a
~100-110 FPS (native 5k) and the 1% was ~70-80 (MASSIVE diff, more than mine)", and "our AMD cause is
definitely wrong".

- **The integrated GPU reading is overturned.** The records of 2026-09-13 and 2026-09-14 parked this bug
  as the width of a laptop that presents through its integrated GPU, with the present path as the part
  that could not be removed here. A desktop with its displays on its only GPU shows the same gap, about
  30 fps under the average against about 20 on the laptop. So the copy across adapters is not what makes
  the width. What stays measured is the coupling, `Present` returning a fixed time after the previous
  frame's GPU end, which hands on whatever varies in the frames' own work on any machine.
- **The owner's lead is the UI.** "what if it's the UI throttling again (the on-screen display, the
  overlays when racing)?" That is the deep dive's periodic piece that was never tested, the game advancing
  and painting one UI view per frame in turn over the HUD and the car's dashboard displays, the same
  rotation the responsive UI already took off menu pages (BUG-024). The main UI view matches the game
  window here (`view #1 1920x1080` in the session logs), so across three monitors at 5K the HUD's turn
  likely carries several times the pixels, which would fit a wider gap on that desktop.
- **Not known yet.** Whether the mod was installed on that desktop, and which counter showed the 1 percent
  low.

The first run is the UI schedule test of [TODO-025](../todos/TODO-025-the-ui-view-rotation-test.md), three
schedules taking turns inside one launch. If none narrows the width, the ETW trace of
[TODO-026](../todos/TODO-026-one-lean-etw-trace-of-the-slow-frames.md) is next.

## The refresh lead, 2026-09-15

In the owner's words, "The 1% was 60fps while the framerate was uncapped. Display was at 60hz. I changed
display to 240hz and now it was 70-90 1%."

- **The game's vsync has three modes.** `enum VSync { Adaptive=0, Off=1, On=2 }` in the settings schema. The
  RHI present at `0x1E3AAD0` passes sync interval 1 for On and sync interval 0 with
  `DXGI_PRESENT_ALLOW_TEARING` for Off. For Adaptive it compares each frame's time since `BeginFrame` with
  the refresh period read once at swap chain creation (`0x1E20A7C`, 1 over the window monitor's rate into
  wrapper `+0x18`), syncs when two frames in a row come under 0.8 of it and tears when two come over 1.2.
  Adaptive would have held that desktop at 60 fps, so it most likely ran Off. This laptop runs Off
  (`v_sync: 1` in the game log).
- **The reading, not measured.** When Windows composes a game's frames instead of flipping them, a frame
  with vsync off still reaches the screen on the refresh, and the frame latency wait at `0x1CE3AA6` can
  only release when a queued frame leaves. Slow frames then round up to refresh steps, 16.7 ms at 60 Hz, a
  60 fps 1 percent low under an uncapped average, and 4.2 ms steps at 240 Hz, where a 9.5 ms frame that
  misses a step lands at 12.5 ms, about 80 fps. Both of the owner's readings fit.
- **This laptop does not hold frames at 165 Hz.** Its frame times do not bunch at multiples of 6.06 ms (the
  `fix1-ai30-20260913` race has 13.0 percent within 0.4 ms of a multiple against 13.2 by chance), the deep
  dive found no lock of present returns to either display, and Windows' variable refresh for games is on
  here (`VRROptimizeEnable=1`). It runs below its refresh, the desktop at 60 Hz ran above it.
- **The 60 fps pile in P3 is the game's background limit.** A window that is not activated is capped at
  `occluded_frame_rate_limit` 60 (`Entering occluded state (isMinimized=false isActivated=false)` in the game
  log), and P3's frames inside those windows form a sharp spike at 16.6 to 16.7 ms. The main loop's
  limiter (`0x74D303` to `0x74D435`) targets 16.667 ms for menus, 1000 over the limit when occluded, and
  nothing in gameplay. The 60.000 Hz clock the deep dive found while driving stays unnamed.

Setting this laptop's monitor to 60 Hz puts it in the desktop's condition. [TODO-028](../todos/TODO-028-the-refresh-hold-test.md)
runs that in the same launch as TODO-025, windowed and then fullscreen.

## The UI schedule and 60 Hz run, 2026-09-15

`logs/hud-refresh-20260915`, build `ca09393` with `hud_schedule_test=1`, Ferrari 296 GT3 at the Red Bull Ring
GP. The HUD changed schedule every 10 seconds in shuffled sets of three. Three stints, 165 Hz windowed for
7.5 minutes, then the monitor at 60 Hz (the game log's `WM_DISPLAYCHANGE` at 17:36:56) windowed for 4.7
minutes, then fullscreen at 60 Hz (`Window updated: 1920x1080, Fullscreen: Yes`) for 3.3 minutes. The first
second of every turn, anything off `hud.html` and the 20 seconds after each return to the track are dropped.
"Local" is frame time over its 101 frame median.

The 165 Hz stint, 43,627 frames.

| schedule | average | p99 | slowest 1% mean | p99 over median | p99 local | 3 frame ripple | frames over 1.2 local |
|---|---|---|---|---|---|---|---|
| the game's rotation | 99.4 fps | 13.37 ms, 74.8 fps | 71.4 fps | 1.324 | 1.241 | 1.00 ms | 334 |
| the HUD every frame, the displays taking turns | 99.2 fps | 12.23 ms, 81.8 fps | 79.1 fps | 1.216 | 1.177 | 0.04 ms | 99 |
| every view every frame | 98.3 fps | 12.93 ms, 77.3 fps | 73.9 fps | 1.284 | 1.181 | 0.05 ms | 91 |

- **The rotation is a real part of the width.** The HUD every frame narrows p99 over the local median by 0.064
  and lifts the 1 percent low by 7 fps at the same average, 10.06 against 10.08 ms a frame. The 60 Hz stints
  agree, 1.208 to 1.152 windowed and 1.216 to 1.144 fullscreen. The frame to next frame swing drops from 1.33
  to 0.76 ms. Every view every frame removes the ripple as well but costs 0.1 ms a frame for the displays and
  keeps a wider p99. At the Red Bull Ring GP the rotation is about 30 percent of the gap between the average
  and the 1 percent low, 24.6 fps down to 17.4, and the owner's HUD lead holds.
- **The HUD's own work is not the cost.** Updating it three times as often adds 0.02 ms a frame, so the ripple
  came from when the rotation made work land, not from how much there was.
- **The 60 Hz clock was in this launch** and weakens with a steady schedule, Rayleigh Z at 60.000 Hz of the
  long frames 15.0 with the rotation, 10.0 with the HUD every frame and 1.9 with every view, against 0.7 to 3.2
  at 57.3 Hz.
- **This laptop does not hold frames for the refresh, even above it.** At 60 Hz, windowed and fullscreen, no
  frame lands within 0.4 ms of a 16.67 ms step, the histograms end by 15 to 16 ms and the averages stay at
  94 to 95 fps. The owner saw the same, "Wierd that the 60hz bug did not happen in my laptop". This laptop
  presents through its AMD adapter, which evidently does not tie the frame latency wait to that display's
  refresh (inference), so the desktop's 60 fps lows belong to a display wired to the rendering GPU and cannot
  be reproduced here.

The fix to drive is the HUD every frame with the displays taking turns, on by default in the responsive UI.

## The fix drive, 2026-09-15

`logs/hud-fix-drive-20260915`, build `2483f0b` with the HUD every frame by default, Ferrari 296 GT3 at the
Red Bull Ring GP, an out lap and laps of 1:41.1, 1:38.2 and 1:37.7, no pause between 17:55:37 and 18:02:47.
The owner reads the NVIDIA app's overlay (Alt+R). In the owner's words, "The 1% was ok at the start and then
tanked for some reason and became spotty. I could maybe tell it was smoother but it might be a placebo
effect."

| from the HUD | average | p99 | slowest 1% mean | p99 local | 3 frame ripple | 60.000 Hz Z |
|---|---|---|---|---|---|---|
| 20 to 50 s | 113.2 fps | 88.0 fps | 83.0 fps | 1.313 | 0.01 ms | 30.5 |
| 50 to 230 s, 30 s blocks | 95.1 to 103.2 fps | 75.1 to 84.8 fps | 69.7 to 80.9 fps | 1.163 to 1.225 | 0.01 to 0.04 ms | 3.1 to 10.9 |
| 230 to 430 s, 30 s blocks | 91.8 to 100.6 fps | 73.1 to 83.7 fps | 69.8 to 79.9 fps | 1.162 to 1.193 | 0.01 to 0.03 ms | 0.0 to 4.2 |

- **The fix held for the whole drive.** The 3 frame ripple stayed at 0.01 to 0.04 ms against 1.00 ms with the
  game's rotation in the run before, and the width against the local median stayed where the HUD every frame
  had it in that run.
- **The 1 percent low fell with the average.** The average went from 113 fps in the first half minute to 92 to
  103 fps from the second minute on, and p99 went with it. The width did not grow. That fits the GPU heating
  up two minutes into a lap, as on every run of this laptop ([thermal-throttle-dominates-lap-fps](../memory/thermal-throttle-dominates-lap-fps.md)),
  though no GPU sampler ran to show it. Block to block the 1 percent low moves 73 to 85 fps with the part of
  the lap, which a short window overlay shows as spotty.
- **What the fix buys against that.** At an average near 99 fps the drive's blocks read about 81 fps at p99,
  against 74.8 for the game's rotation at 99.4 fps in the run before, the same gain as the A/B, smaller than
  the 15 to 20 fps the average loses as the card heats.
- **The 60 Hz clock was strongest in the first half minute** and faded over two minutes.
- **The overlay's dips are not in the game's presents.** The owner saw the NVIDIA overlay's 1 percent low drop to
  about 30 fps at times with 100 fps showing, "Or the nvidia overlay is wrong". The mod times every `Present` and
  `Present1` call on entry (`OnPresent` in `src/render/frame_stats.cpp`). Of 40,323 driving frames none was over
  25 ms, the longest 21.7 ms, and a 1 percent low over any rolling second never went under 46 fps (median 81.7).
  So a 30 fps reading is either frames reaching the screen late or dropped after `Present`, on the hand off to
  the AMD display, or the overlay counting something else. Present times cannot tell which. PresentMon records
  both the present and the display change per frame.

The owner's 5070 desktop is gone, so no run there. The next step is naming the rest of the width, the
60.000 Hz clock and the renderer's spread, with the GPU sampler running and the lean trace of
[TODO-026](../todos/TODO-026-one-lean-etw-trace-of-the-slow-frames.md).

## PresentMon against the overlay, 2026-09-15

`logs/presentmon-drive-20260915`, PresentMon 2.5.1 (Intel signed, `--track_hybrid_present`) beside the mod
with the HUD fix, the owner's fan curve on, cut short by a power cut. In the owner's words, "nvidia overlay
shows a constant 30-50 fps gap", and mid lap 1 "the 1% according to nvidia was 55 when fps was 110".

- **The overlay and the mod agree.** PresentMon's present intervals match the mod's frames block for block, at
  18:20:36 both read 112.9 fps with the slowest 1 percent averaging 59.6 fps, the owner's reading. The overlay's
  1 percent low matches the slowest 1 percent's mean. p99, which the tables above lead with, reads 10 to 15 fps
  higher.
- **The path to the screen is clean.** 19,077 of 19,197 frames are `Hardware: Independent Flip` and 120
  `Composed: Flip`, `HybridPresent` is 0 on every frame, tearing is allowed and no frame went undisplayed. This
  laptop scans the NVIDIA frames out on the AMD display without composing them, which is why 60 Hz held nothing.
- **A cool card lifts the median, not the slow frames.** Without the heat the average rose to 104 to 119 fps and
  the median frame to 9.1 ms, while the slowest 1 percent stayed at 57 to 71 fps, so the gap in fps grew.
- **The slow frames are heavy frames.** Over 15,738 driving frames, the slowest 1 percent (15.7 ms) against the
  median frames (9.1 ms), in PresentMon's split: CPU busy, from the previous `Present` returning to this call
  and the mod's Reflex sleep included, 12.37 against 6.00 ms, time inside `Present` 0.42 against 2.97, GPU busy
  11.70 against 9.02, GPU wait 0.02 for both. The two frames before are ordinary (CPU busy 6.1 to 6.2, GPU busy
  8.3 to 9.5), and the frame after waits 5.08 ms in `Present` for the heavy frame's GPU work. 124 of the 158
  have over 10 ms of CPU busy. The frame carries more work on both sides, which a longer Reflex sleep would not.
- **Not streaming, not the mod's tick, no fixed rhythm.** Streaming requests land in the slow frame or the two
  before in 17.6 percent of slow frames against 13.9 of all frames, tile requests in the same frame 4.5 against
  2.7. Against the mod's once a second tick the Rayleigh Z is 0.1. Slow frames come 0.25, 0.59 and 1.2 s apart
  at the quartiles.

What the heavy frames do is the next question, render thread samples in heavy frames against ordinary ones,
which the lean trace of TODO-026 records without the in process sampler's shift.

## The traced drive, 2026-09-15

`logs/trace-drive-20260915`, the lean profile recording from the pit menu through an out lap, laps of 1:43.0
and 1:36.3 and the quit, 5 min 25 s and 5.6 GB with no lost events, the HUD fix, the fan curve and the NVIDIA
overlay on, no PresentMon. In the owner's words, "this time it didnt even drop that low on the 1%".

- **A good launch.** Over 31,327 driving frames the average was 107.5 fps, p99 84.3 fps and the slowest 1 percent
  79.8 fps, with the longest frame 15.6 ms. In 30 s blocks the slowest 1 percent held 77 to 84 fps. The
  PresentMon drive an hour before, at a similar average, had its slowest 1 percent at 57 to 71 fps and frames of
  18 to 24 ms. So the heavy frames vary by launch, the way the deep dive saw the 60 Hz clock come and go.
- **The trace names the threads.** The game presents from `GameThread` (the main loop at `0x74C6A0` in
  `PlatformClient.cpp`), beside `Physics`, `Render Worker 0` to `4` and two `Resource Manager Worker`s. The exe
  loads at `0x7FF6D7070000`. The CPU is a Ryzen 9 5900HX, 8 cores, logical processors paired per core.
- **Decoding it is heavy.** A second of trace is about 200 MB of text with stacks. Each `-range` pass scans the
  file for about a minute, so the trace is decoded in 30 s pieces, four at a time, keeping only the game's
  samples, the game thread's stacks and context switches, the Physics thread's switches and the presents.

What the trace shows, over 31,378 driving frames cut at the game thread's DXGI `Present` starts (median 9.30 ms,
p99 11.88 ms), the 314 heavy frames at or over p99 (12.55 ms) against the 12,372 ordinary frames within 0.5 ms of
the median.

| per frame | heavy | ordinary | extra |
|---|---|---|---|
| game thread running | 8.38 ms | 6.76 ms | +1.62 |
| waiting in the present path (`0x1E3AAD0`), woken by a DPC | 3.24 ms | 2.50 ms | +0.74 |
| waiting on the frame latency object in `BeginFrame` (`0x1CE3A70`), woken by `dwm.exe` | 0.69 ms | 0.00 ms | +0.69 |
| sleeping in the present path, the mod's Reflex sleep | 0.19 ms | 0.01 ms | +0.18 |
| on the Physics thread's core | 0.32 ms | 0.14 ms | +0.18 |

- **No single culprit in this launch.** The extra running time spreads over the main loop's frame step
  (`0x896623` under `MainLoop` `0x74C6A0`, +0.61), the jobs the game thread runs itself under `0x279F830`
  (+0.59), the texture streamer under the mod's hooks (+0.41 across both), the NVIDIA driver (+0.18), the kernel and
  `ntdll` (+0.32), Cohtml (+0.15) and the job scheduler's spin lock `0x279FA90` (+0.10). The mod's own code is not
  among the game thread's top 20 modules for the whole trace.
- **Waits are woken on time.** After the GPU's DPC or `dwm.exe` readies the game thread it runs within 0.017 ms on
  average, so there is no scheduling delay and no processor shortage, as the deep dive found.
- **A full frame queue waits on the compositor.** In ordinary frames the frame latency object is already signalled
  at `BeginFrame`. In 71 of the heavy frames it was not, and the thread that released it was `dwm.exe`, so after a
  frame's GPU work runs long the next frame waits for the compositor to take the one before.
- **Heavy frames bunch in one stretch of the lap.** 53 of the 1,039 frames between 160 and 170 s and 36 of the 1,000
  between 270 and 280 s were heavy, both about 55 to 70 s into a lap, against 1 to 19 in other 10 s windows. Every
  frame there is slower (99.9 to 103.8 fps against 99 to 117 elsewhere) with no more streaming (216 and 337 tile
  requests against 142 to 329), so it is the scenery of that stretch. Its slowest 1 percent still held 73.6 to
  75.9 fps.
- **The bad launch is not in this trace.** The PresentMon drive's heavy frames came everywhere with twice the CPU
  work. Two things differ: that launch, and the PresentMon capture itself, which was started with
  `--stop_existing_session` while the NVIDIA overlay runs a `PresentMon_x64.exe` service of its own. Which one it
  was is not known.

## The instant drops, 2026-09-15

`logs/trace-long-20260915`, the lean trace over an out lap and the start of lap one. The owner saw the overlay drop
"twice but only for an instant". One was a single 35.0 ms frame, a 28.5 fps 1 percent low over its second, and
the trace puts it on the HUD: the UI worker restyled a large part of the HUD page for about 24 ms and the game
thread waited for it at EvoUi's end of frame. The details are
[BUG-029](BUG-029-the-hud-restyles-most-of-its-page-while-driving.md). The other was a 16.5 ms frame with HUD script
on the UI worker. So the owner's "30 at times" readings are single HUD frames, and the UI probe names what sets
them off.

The probe named it, a HUD part leaving the HUD's top level (the wrong way label, which also flickers around the
pit lane), and Cohtml restyling everything under a parent on any child removal. The responsive UI's child
removal fix took it out: the drive of 2026-09-15 evening had five wrong way episodes with no frame over 12.8 ms
around them and no restyle over 15 ms while driving. BUG-029 is fixed.

## The gap after the HUD fixes, 2026-09-15

Driving time only (pit exit plus 3 s to pit entry or the pause), the Ferrari 296 GT3 at the Red Bull Ring GP. The
three probe runs carry the UI probe's own hooks, so they compare with each other more than with the rest.

| Session | Probe | Driving | Avg fps | 1% low | 0.1% low | 1 s windows, median, 10th percentile, worst | Frames of 16 ms or more |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `hud-fix-drive` | off | 411 s | 98.6 (heat) | 74.3 | 63.8 | 81.4, 73.9, 46.1 | 11 |
| `trace-drive` | off, traced | 293 s | 107.5 | 79.7 | 70.6 | 83.5, 74.0, 64.3 | 0 |
| `probe-hud` | on | 1,069 s | 108.5 | 66.3 | 26.8 | 83.8, 69.8, 11.1 | 76 |
| `probe-hud-b` | on | 1,032 s | 108.6 | 81.5 | 71.8 | 87.6, 77.2, 29.0 | 1 |
| `children-fix` | on, BUG-029 fixed | 614 s | 108.2 | 83.0 | 74.0 | 86.4, 77.2, 62.0 | 1 |

- **No drop to 30 is left.** The worst one second window of the last run held 62 fps, against 29 and 11 in the runs
  before the fix.
- **What is left is spread out.** The slowest 1 percent of the last run averages 12.05 ms against a 9.24 ms median.
  Every 10 s stretch of the drive holds some of them, they carry 0.3 ms more UI frame end wait (0.69 against 0.38 ms)
  and four and a half times the texture tile requests (0.90 against 0.20 a frame).
- **A run with the probe off** is the player's number, the last clean one is `trace-drive` at 79.7.

## Verification

Absent.
