---
name: bugs-index
kind: doc
description: the defect tracker's index, open bugs first
updated: 2026-09-05
links: [agent-index, spec-bugs]
---

# Bugs Index

## Open

bug

- [BUG-001-texture-low-mip-shown-before-streaming](BUG-001-texture-low-mip-shown-before-streaming.md), low mip visible for about a second, I/O is not the limit
- [BUG-002-fps-drop-entering-new-track-sections](BUG-002-fps-drop-entering-new-track-sections.md), GPU pinned and thermally throttled during the lap
- [BUG-009-one-percent-lows-far-below-average](BUG-009-one-percent-lows-far-below-average.md), alternation fixed by the latency cap, the section spread remains
- [BUG-012-pit-lane-return-freezes-over-a-second](BUG-012-pit-lane-return-freezes-over-a-second.md), back to pits loads the dynamic track preset on the render thread, 1.3 to 1.5 s frames
- [BUG-013-one-percent-lows-drop-after-window-or-input-switch](BUG-013-one-percent-lows-drop-after-window-or-input-switch.md), 1 percent low falls after a window or input device switch, input polling is the first suspect to measure

nit

- [BUG-006-distant-objects-pop-in](BUG-006-distant-objects-pop-in.md), grass draw distance, LOD scales at 1.0

## Fixed

- [BUG-003-menu-icons-stop-rendering](BUG-003-menu-icons-stop-rendering.md), VRAM starvation by the staging buffers, fixed by the 128 MB cap
- [BUG-004-crash-on-car-or-track-change](BUG-004-crash-on-car-or-track-change.md), VRAM exhaustion on scene switch, same fix, owner verified
- [BUG-005-crash-on-startup](BUG-005-crash-on-startup.md), VRAM exhaustion on first load, same fix, owner verified
- [BUG-007-blurry-road-and-textures](BUG-007-blurry-road-and-textures.md), small race pool plus texture quality High, fixed by the fixed pool and Ultra
- [BUG-010-texture-pool-shrinks-on-race-load-and-restart](BUG-010-texture-pool-shrinks-on-race-load-and-restart.md), pool sized during scene transitions, fixed by fixed pool sizes, owner verified
