---
name: BUG-002-fps-drop-entering-new-track-sections
kind: bug
description: frame rate falls from about 90 to the 60s and 70s during a lap, GPU is pinned and thermally throttled
updated: 2026-09-05
links: [lap-2026-09-05-nordschleife, TODO-002-general-optimisation-pass, thermal-throttle-dominates-lap-fps]
status: open
severity: bug
area: render
reported: 2026-09-05
parent:
---

## Problem

Owner wording: "sometimes when i enter new sections, it drops to 60 fps (from 90)".

## Evidence

Lap session 2026-09-05, driving window 17:12:30 to 17:26:50:

- Per minute average fps 75 to 83, per minute minimum 60 to 72. No frame over 23 ms while
  driving, every frame over 33 ms belongs to loading or the menu.
- GPU utilisation 95 to 100 percent the whole time. Core clock 1838 MHz in the first minute at
  85 °C, then 1490 to 1560 MHz for the rest of the lap at 87 to 88 °C. The driver's software
  thermal slowdown flag is active in 98 percent of the seconds. Power sits at 86 to 100 W of a
  159 W peak, so the limit is heat, not the power cap.
- CPU: game process 22 percent of all cores, system 32 percent. Not a CPU limit.
- VRAM: peak 4592 MB of a 5226 MB budget, never within 100 MB of it. Not paging.
- Every slow cluster in the report has `pso 0` or single digits, so shader compilation is not
  involved after the track load.

Reading: the scene is GPU bound and the clock loses about 19 percent to heat within two minutes
of driving. Heavier sections then land below 70.

## Fix

Absent. Two levers, see TODO-002: less GPU work per frame (upscaler preset and the four Ultra
settings) and more cooling headroom (fan profile).

## Verification

Absent.
