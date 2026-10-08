---
name: reference-machine-has-no-direct-gpu-display
kind: memory
description: the reference machine renders on its NVIDIA GPU and presents through its integrated AMD GPU, a path that cannot be tested away on it, and a machine without that path showed it is not what makes the 1 percent lows
updated: 2026-10-08
links: [BUG-009-one-percent-lows-far-below-average, TODO-010-resume-the-one-percent-low-hunt, thermal-throttle-dominates-lap-fps, one-percent-lows-2026-09-14]
type: project
---

The reference machine, an RTX 3060 Laptop 6 GB beside an AMD integrated GPU, renders on the NVIDIA
GPU and presents through the AMD one. Every display it drives is an output of the AMD adapter, as the
game's `[Monitor]` lines and the mod's `[display]` warning show in every session, so every frame is
copied across before it is shown. The machine has no way to switch that path off or to drive a
display from the NVIDIA GPU.

It matters because the cross adapter present path was a lead for the 1 percent lows (BUG-009, the
render thread spent 0.9 ms of a median frame and up to 4 ms of a slow one in it on 2026-09-05), and
it cannot be removed on this machine to test it. A machine with no integrated GPU showed the same gap
on 2026-09-15, so this path is not what makes the lows
([one-percent-lows-2026-09-14](../docs/research/one-percent-lows-2026-09-14.md)).

Apply it by treating the present path as fixed hardware on the reference machine, and by never
planning or asking for a test there that needs a display on the rendering GPU. A measurement of that
path needs a different machine or a report from a player who has one.
