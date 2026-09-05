---
name: bugs-index
kind: doc
description: the defect tracker's index, open bugs first
updated: 2026-09-05
links: [agent-index, spec-bugs]
---

# Bugs Index

## Open

breaks

- [BUG-004-crash-on-car-or-track-change](BUG-004-crash-on-car-or-track-change.md), game ends on car or track change, no dump found yet
- [BUG-005-crash-on-startup](BUG-005-crash-on-startup.md), some launches end before the menu

bug

- [BUG-001-texture-low-mip-shown-before-streaming](BUG-001-texture-low-mip-shown-before-streaming.md), low mip visible for about a second, I/O is not the limit
- [BUG-002-fps-drop-entering-new-track-sections](BUG-002-fps-drop-entering-new-track-sections.md), GPU pinned and thermally throttled during the lap
- [BUG-007-blurry-road-and-textures](BUG-007-blurry-road-and-textures.md), soft surfaces, settings and upscaler related
- [BUG-008-ui-opens-slowly-with-loading-spinner](BUG-008-ui-opens-slowly-with-loading-spinner.md), UI pages load on demand, preload flag untested

nit

- [BUG-006-distant-objects-pop-in](BUG-006-distant-objects-pop-in.md), LOD distance scales at 1.0

## Fixed

- [BUG-003-menu-icons-stop-rendering](BUG-003-menu-icons-stop-rendering.md), VRAM starvation by the staging buffers, fixed by the 128 MB cap
