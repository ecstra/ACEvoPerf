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

## Fix

Partial: `max_frame_latency=1` is the default now (DEC-006). The remaining gap is the GPU load
spread between sections. Remaining candidates: a frame rate cap just under the typical rate, the
`gpu-relief` settings profile, and cooling.

## Verification

Absent.
