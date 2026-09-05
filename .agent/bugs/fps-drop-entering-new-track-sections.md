---
title: Frame rate drops from about 90 to 60 when entering new track sections
status: investigating
severity: major
reported: 2026-09-05
updated: 2026-09-05
source: user, "sometimes when i enter new sections, it drops to 60 fps (from 90)"
---

## Symptom

On Nordschleife the frame rate holds around 90 and then sits at about 60 for a while after entering
a new part of the track. It recovers later.

## Evidence

- The game log of every session so far contains `PSO Cache: N pipeline requests never completed,
  re-enabling them` warnings, so pipeline state objects are still being compiled during play.
  The engine flag `enable_pso_cache` defaults to false in this build and is now set to true by the
  mod. The first lap after enabling it still compiles, later laps should not.
- Lap telemetry (per second fps, hitch counts, tile and mesh streaming volume, VRAM use against
  budget, CPU load, GPU clocks and throttle reasons) is being collected in the session folder
  named in `.agent/handover/2026-09-05-telemetry-lap.md`.

## Suspects

- Shader compilation on first sight of new materials. Confirmed if the game log shows a burst of
  PSO creation lines at the same clock time as the drop.
- GPU power or thermal limit on the laptop as the scene gets heavier. Confirmed if the GPU sampler
  shows clocks falling or a throttle reason flag while utilisation stays at 100 percent.
- VRAM over the budget with the larger streaming pools, causing residency paging. Confirmed if
  `vram_used_mb` reaches `vram_budget_mb` in the timeline at the time of the drop.
- Global illumination probe baking for the new area (`gibake_probes_per_frame`, default 16).
- Simply more geometry and vegetation in those sections (a real GPU bound scene, no anomaly).

## Fix

Not yet.
