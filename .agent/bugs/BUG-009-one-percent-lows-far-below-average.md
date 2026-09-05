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
  ruined by the UI: every pause menu and settings open is a 100 to 300 ms document reload
  (BUG-008) and the settings page pushed VRAM to 5337 MB, over the 5226 MB budget. A build and
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
  in both. The return costs one HUD document reload (BUG-008, worst frame 131 ms). After that
  the car is in a section that streams 819 MB of tiles and uploads 669 MB of meshes and
  textures in a minute, with the GPU clock 60 MHz lower, and that is where the 1 percent low
  sits at 53. The window switch itself leaves the pacing intact, the section does the rest. So
  the owner's reading is the reload stall plus BUG-002, not a new defect.

## Fix

Partial: `max_frame_latency=1` is the default now (DEC-006). The remaining gap is the GPU load
spread between sections, and any UI page opened during a stint (BUG-008). Remaining candidates:
a frame rate cap just under the typical rate, the `gpu-relief` settings profile, and cooling.

## Verification

Absent.
