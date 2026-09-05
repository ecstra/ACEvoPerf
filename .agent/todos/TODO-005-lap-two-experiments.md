---
name: TODO-005-lap-two-experiments
kind: todo
description: the ordered list of single variable experiments for the second lap
updated: 2026-09-05
links: [BUG-001-texture-low-mip-shown-before-streaming, BUG-002-fps-drop-entering-new-track-sections, BUG-006-distant-objects-pop-in, BUG-007-blurry-road-and-textures, lap-2026-09-05-nordschleife]
status: open
by: agent
area: streaming
born: 2026-09-05
done:
---

## What

Changes to try against the lap of 2026-09-05, one at a time so each effect is attributable.

Mod side, no visual cost:

1. Tile queue priority raised from `NORMAL` to `REALTIME` through the new `tile_queue_priority`
   ini key (commit for "tile_queue_priority option"). Smoke tested 2026-09-05 17:38, queue created
   at priority 2 without error.
2. Engine flag `texture_tier0=true`. Smoke tested the same run: flag written, no error, tile pool
   and VRAM identical in the menu scene, so its effect has to be judged on a lap (texture
   sharpness, tile traffic, VRAM).
3. Engine flag `ui_force_resource_preloading=true` for BUG-008. Smoke tested: the game preloads
   1037 UI files (181 MB) at start.
4. `force_canonical_pool_sizes=true` so revisited sections stay resident (1433 MB pools measured,
   margin to check in the timeline).

Lap two of 2026-09-05 ran items 1 to 3 together with the owner's settings unchanged. Result:
item 2 is a regression (tile traffic fell to 1.3 GB in four minutes, whole minutes at 1 MB, the
car stayed at low detail, the owner's screenshot shows base mips on the livery), item 3 changed
nothing the owner could feel, item 1 showed no measurable difference. All three are off again.
Item 4 remains open, and the UI lag (BUG-008) needs a different cause than resource loading.

Settings side, owner's trade, for lap three:

5. DLSS Quality instead of Ultra Quality.
6. Clouds Ultra to High, volumetrics Ultra to High, motion blur Ultra to Medium, grass Ultra to
   High.
7. `texturePoolSize` Ultra and `textureQuality` Ultra with the mod's larger pool, for sharpness.
8. LOD custom mode with `mainLodDistanceScale` 1.25 for BUG-006.

## Done when

Each numbered item has a before and after number from `tools/telemetry_report.py` written into
the research doc, and the winners are in the default ini or the recommended profile.
