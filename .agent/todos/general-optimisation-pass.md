---
title: General optimisation pass on top of the streaming fix
status: open
created: 2026-09-05
updated: 2026-09-05
source: user, "And ofc, general optimization needed as well."
---

## Done when

A default configuration (mod ini plus recommended video settings) that measurably raises the lap
average fps or lowers the hitch count on the 6 GB laptop GPU without visible quality loss, and a
second profile for users who prefer sharper textures over frame rate. Both documented.

## Steps

1. Baseline from the lap telemetry: average fps, one percent low, hitch count. Check: numbers
   written into this file.
2. Engine flags with a plausible gain and no visual cost: `gibake_probes_per_frame`,
   `car_update_*_budget`, `disable_dynamic_track`. Check: each tested on its own for one lap.
3. Video settings with high VRAM or GPU cost on this class of GPU: clouds Ultra at 1024,
   volumetrics Ultra, motion blur Ultra, grass Ultra. Check: frame cost per setting.
4. Swap chain latency cap (`max_frame_latency`) for input feel. Check: no fps regression.

## Notes

- The laptop has no MUX switch, the window always goes through the integrated GPU. That cost is
  fixed and outside the mod.
