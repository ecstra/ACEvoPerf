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

1. Tile queue priority raised from `NORMAL` to `REALTIME` in `FactoryProxy::CreateQueue`, to cut
   queueing behind the two other queues during bursts.
2. Engine flag `texture_tier0=true`, first checked alone in the menu for tile pool use and VRAM,
   then a lap.
3. `force_canonical_pool_sizes=true` so revisited sections stay resident (1433 MB pools measured,
   margin to check in the timeline).

Settings side, owner's trade:

4. DLSS Quality instead of Ultra Quality.
5. Clouds Ultra to High, volumetrics Ultra to High, motion blur Ultra to Medium, grass Ultra to
   High.
6. `texturePoolSize` Ultra and `textureQuality` Ultra with the mod's larger pool, for sharpness.
7. LOD custom mode with `mainLodDistanceScale` 1.25 for BUG-006.

## Done when

Each numbered item has a before and after number from `tools/telemetry_report.py` written into
the research doc, and the winners are in the default ini or the recommended profile.
