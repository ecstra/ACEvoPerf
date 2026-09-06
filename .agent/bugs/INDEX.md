---
name: bugs-index
kind: doc
description: the defect tracker's index, open bugs first
updated: 2026-09-06
links: [agent-index, spec-bugs]
---

# Bugs Index

## Open

bug

- [BUG-002-fps-drop-entering-new-track-sections](BUG-002-fps-drop-entering-new-track-sections.md), GPU pinned and thermally throttled during the lap
- [BUG-009-one-percent-lows-far-below-average](BUG-009-one-percent-lows-far-below-average.md), the render thread hands its main batch to the GPU 3 to 5 ms late in heavy views, everything else ruled out over nineteen laps, parked with its leads in TODO-010
- [BUG-012-pit-lane-return-freezes-over-a-second](BUG-012-pit-lane-return-freezes-over-a-second.md), back to pits loads the dynamic track preset on the render thread, 1.3 to 1.5 s frames
- [BUG-013-one-percent-lows-drop-after-window-or-input-switch](BUG-013-one-percent-lows-drop-after-window-or-input-switch.md), pause and HUD reload stalls through a rolling counter plus the device rebuild on a device change, diagnostics removed, the device stays to be named

## Won't fix

- [BUG-001-texture-low-mip-shown-before-streaming](BUG-001-texture-low-mip-shown-before-streaming.md), the mip a surface gets at a distance is the engine's choice on every card, the streaming path answers in 9 ms bursts
- [BUG-006-distant-objects-pop-in](BUG-006-distant-objects-pop-in.md), grass fades at 30 m and trees switch at 100 m by the package's own values on every card, the Custom LOD setting moves them at a cost

## Fixed

- [BUG-003-menu-icons-stop-rendering](BUG-003-menu-icons-stop-rendering.md), VRAM starvation by the staging buffers, fixed by the 128 MB cap
- [BUG-004-crash-on-car-or-track-change](BUG-004-crash-on-car-or-track-change.md), VRAM exhaustion on scene switch, same fix, owner verified
- [BUG-005-crash-on-startup](BUG-005-crash-on-startup.md), VRAM exhaustion on first load, same fix, owner verified
- [BUG-007-blurry-road-and-textures](BUG-007-blurry-road-and-textures.md), small race pool plus texture quality High, fixed by the fixed pool and Ultra
- [BUG-010-texture-pool-shrinks-on-race-load-and-restart](BUG-010-texture-pool-shrinks-on-race-load-and-restart.md), pool sized during scene transitions, fixed by fixed pool sizes, owner verified
