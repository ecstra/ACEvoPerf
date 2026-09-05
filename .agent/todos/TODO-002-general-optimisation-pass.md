---
name: TODO-002-general-optimisation-pass
kind: todo
description: raise the lap frame rate on the 6 GB laptop GPU without visible quality loss
updated: 2026-09-05
links: [BUG-002-fps-drop-entering-new-track-sections, lap-2026-09-05-nordschleife, settings-files]
status: open
by: owner
area: render
born: 2026-09-05
done:
---

## What

"And ofc, general optimization needed as well."

## Why

The lap of 2026-09-05 shows the GPU pinned at 100 percent and thermally throttled to about
1500 MHz. Frame rate follows GPU work per frame and cooling, nothing else was near a limit.

## Done when

A default ini plus a recommended settings profile that lifts the per minute average above the
current 75 to 83 fps on the same lap, measured with the timeline, and a second profile for
sharper textures. Candidates, each to be measured alone for one lap: DLSS Quality instead of
Ultra Quality (about 22 percent fewer pixels), clouds Ultra to High, volumetrics Ultra to High,
motion blur Ultra to Medium, grass Ultra to High, `gibake_probes_per_frame` 16 to 8, and a fan
profile that keeps the GPU under its 87 °C slowdown point.
