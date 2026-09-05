---
name: BUG-009-one-percent-lows-far-below-average
kind: bug
description: the 1 percent low frame rate sits about 20 fps under the displayed average
updated: 2026-09-05
links: [lap-2026-09-05-nordschleife, BUG-002-fps-drop-entering-new-track-sections, telemetry]
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
  in windowed mode.

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

## Fix

Absent. The cap is off (DEC-008). GI, the dynamic track and the heavy settings are ruled out
as the variance source, the swap chain queue too. The slow frames are the render thread waiting,
in the job scheduler's spin lock and in kernel waits. Next: the frames CSV now carries `wait_ms`
and `fence_ms`, every wait of the render thread from any module timed and the part spent on
D3D12 fence events (the GPU) told apart, with a `[wait]` log line per handle and minute. That
decides between the GPU (fence waits grow in slow frames, then the fix is on the GPU side or in
the queue depth) and the worker threads (other handles grow, then thread priorities and
affinities of the job workers are the lever). `fps_limit` remains the direct pacing tool,
declined by the owner for now.

## Verification

Absent.
