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
- Periodicity analysis of `acevo_perf_frames.csv` is recorded below the evidence once run.

## Fix

Absent.

## Verification

Absent.
