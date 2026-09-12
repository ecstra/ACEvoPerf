---
name: bugs-index
kind: doc
description: the defect tracker's index, open bugs first
updated: 2026-09-12
links: [agent-index, spec-bugs]
---

# Bugs Index

## Open

bug

- [BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache](BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache.md), two Overtake reviewers report unlit trees at night and name `enable_pso_cache`, a default the mod turns on, the owner's own log shows the cache failing to deliver pipelines after a game update
- [BUG-016-vram-overhead-grows-across-scene-loads](BUG-016-vram-overhead-grows-across-scene-loads.md), the overhead part of the VRAM report climbs 24 MB to 360 MB over thirteen loads while resource memory per scene stays identical, the leading explanation for textures going blurry after several reloads
- [BUG-017-trackside-big-screens-blurry](BUG-017-trackside-big-screens-blurry.md), the big screen flipbook ships at a 2x cook shrink with 3 of 12 mip levels, and its 8 by 8 grid makes any coarse mip cost eight times the detail, 64 by 64 per frame on a full size screen
- [BUG-018-whole-scene-low-detail-for-a-second-after-load](BUG-018-whole-scene-low-detail-for-a-second-after-load.md), the whole scene is coarse for a second or two after the curtain lifts, at start-up and at track entry, distinct from BUG-001, measurement armed
- [BUG-019-car-physics-rebuilds-every-tyre-model-five-times](BUG-019-car-physics-rebuilds-every-tyre-model-five-times.md), the 296 GT3 spends 3.52 s building twenty tyre models for four distinct results, on the serial chain that ends the session load 2.05 s after streaming finishes
- [BUG-002-fps-drop-entering-new-track-sections](BUG-002-fps-drop-entering-new-track-sections.md), GPU pinned and thermally throttled during the lap
- [BUG-009-one-percent-lows-far-below-average](BUG-009-one-percent-lows-far-below-average.md), the render thread hands its main batch to the GPU 3 to 5 ms late in heavy views, everything else ruled out over nineteen laps, parked with its leads in TODO-010
- [BUG-013-one-percent-lows-drop-after-window-or-input-switch](BUG-013-one-percent-lows-drop-after-window-or-input-switch.md), pause and HUD reload stalls through a rolling counter plus the device rebuild on a device change, diagnostics removed, the device stays to be named

## Won't fix

- [BUG-014-ui-pages-lag-on-open-switch-and-interaction](BUG-014-ui-pages-lag-on-open-switch-and-interaction.md), document reloads, one frame list builds and per frame relayouts in the game's own UI scripts, measured lever by lever, one overhaul round tried, closed on the owner's word with everything UI related removed
- [BUG-012-pit-lane-return-freezes-over-a-second](BUG-012-pit-lane-return-freezes-over-a-second.md), the 1.2 s is the engine parsing its 63 MB zlib and protobuf track preset at every session start and restart, same on every card
- [BUG-001-texture-low-mip-shown-before-streaming](BUG-001-texture-low-mip-shown-before-streaming.md), the mip a surface gets at a distance is the engine's choice on every card, the streaming path answers in 9 ms bursts
- [BUG-006-distant-objects-pop-in](BUG-006-distant-objects-pop-in.md), grass fades at 30 m and trees switch at 100 m by the package's own values on every card, the Custom LOD setting moves them at a cost

## Fixed

- [BUG-003-menu-icons-stop-rendering](BUG-003-menu-icons-stop-rendering.md), VRAM starvation by the staging buffers, fixed by the 128 MB cap
- [BUG-004-crash-on-car-or-track-change](BUG-004-crash-on-car-or-track-change.md), VRAM exhaustion on scene switch, same fix, owner verified
- [BUG-005-crash-on-startup](BUG-005-crash-on-startup.md), VRAM exhaustion on first load, same fix, owner verified
- [BUG-007-blurry-road-and-textures](BUG-007-blurry-road-and-textures.md), small race pool plus texture quality High, fixed by the fixed pool and Ultra
- [BUG-010-texture-pool-shrinks-on-race-load-and-restart](BUG-010-texture-pool-shrinks-on-race-load-and-restart.md), pool sized during scene transitions, fixed by fixed pool sizes, owner verified
