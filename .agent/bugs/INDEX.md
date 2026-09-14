---
name: bugs-index
kind: doc
description: the defect tracker's index, open bugs first
updated: 2026-09-14
links: [agent-index, spec-bugs]
---

# Bugs Index

## Open

bug

- [BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache](BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache.md), two Overtake reviewers report unlit trees at night and name `enable_pso_cache`, a default the mod turns on, the owner's own log shows the cache failing to deliver pipelines after a game update
- [BUG-016-vram-overhead-grows-across-scene-loads](BUG-016-vram-overhead-grows-across-scene-loads.md), a one time heap fill of about 1.25 GB with the first track and then about 110 MB a track the game keeps, the same passive, page file space the only cost found, the VRAM overhead spike is placement, what grows is unnamed until TODO-023's run
- [BUG-017-trackside-big-screens-blurry](BUG-017-trackside-big-screens-blurry.md), the big screen flipbook ships at a 2x cook shrink with 3 of 12 mip levels, and its 8 by 8 grid makes any coarse mip cost eight times the detail, 64 by 64 per frame on a full size screen
- [BUG-018-whole-scene-low-detail-for-a-second-after-load](BUG-018-whole-scene-low-detail-for-a-second-after-load.md), the whole scene is coarse for a second or two after the curtain lifts, at start-up and at track entry, distinct from BUG-001, measurement armed
- [BUG-019-car-physics-rebuilds-every-tyre-model-five-times](BUG-019-car-physics-rebuilds-every-tyre-model-five-times.md), the 296 GT3's twenty tyre compound builds span 3.52 s on the serial chain that ends the session load, but each build takes about 0.15 ms and the time sits around the compound asset fetch between them
- [BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash](BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash.md), the streamer log line reads the engine's freed allocator at exit, the mod catches it but the game writes a crash report naming the mod
- [BUG-002-fps-drop-entering-new-track-sections](BUG-002-fps-drop-entering-new-track-sections.md), GPU pinned and thermally throttled during the lap
- [BUG-009-one-percent-lows-far-below-average](BUG-009-one-percent-lows-far-below-average.md), the slowest frames are the present path through the integrated GPU coupled to the previous frame's GPU end, spread out renderer code and a ripple from one UI view updated per frame, not a lock or a job, Reflex already evens the alternation, two runs with no build next
- [BUG-013-one-percent-lows-drop-after-window-or-input-switch](BUG-013-one-percent-lows-drop-after-window-or-input-switch.md), pause and HUD reload stalls through a rolling counter plus the device rebuild on a device change, diagnostics removed, the device stays to be named
- [BUG-014-ui-pages-lag-on-open-switch-and-interaction](BUG-014-ui-pages-lag-on-open-switch-and-interaction.md), the menus lag and the settings, controls and vehicle setup pages stall, reopened on the owner's pick and split by the 2026-09-14 deep dive into BUG-024 to BUG-027 and an interaction cost still to name
- [BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three](BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three.md), the pit menu and every page opened from it advance the UI one frame in three, because the engine's every view every frame rule covers only the main menu and pause
- [BUG-025-controls-page-scans-the-page-once-per-new-row](BUG-025-controls-page-scans-the-page-once-per-new-row.md), a bindings group click freezes 250 to 500 ms because each new row scans the whole page once per navigation section while the rows are still outside the document
- [BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open](BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open.md), every vehicle setup open requests the setup twice and builds all its groups twice
- [BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load](BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load.md), 1.2 MB of UI stylesheets read and parsed again at every document load, 38 MB of repeat reads in a 20 minute race session

debt

- [BUG-023-overlay-keeps-a-64-mb-table-copy-for-the-whole-session](BUG-023-overlay-keeps-a-64-mb-table-copy-for-the-whole-session.md), the override layer holds a 64 MB decoded package table for the whole session though the game reads its table in the first seconds

nit

- [BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring](BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring.md), every camera cut of the Red Bull Ring pit menu starts blurry, a cut forces a streamer pass that loads before it drops, often cannot load, then waits a second, the follow up pass of TODO-024 is the correction to test

## Won't fix

- [BUG-012-pit-lane-return-freezes-over-a-second](BUG-012-pit-lane-return-freezes-over-a-second.md), the 1.2 s is the engine parsing its 63 MB zlib and protobuf track preset at every session start and restart, same on every card
- [BUG-001-texture-low-mip-shown-before-streaming](BUG-001-texture-low-mip-shown-before-streaming.md), the mip a surface gets at a distance is the engine's choice on every card, the streaming path answers in 9 ms bursts
- [BUG-006-distant-objects-pop-in](BUG-006-distant-objects-pop-in.md), grass fades at 30 m and trees switch at 100 m by the package's own values on every card, the Custom LOD setting moves them at a cost

## Fixed

- [BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles](BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles.md), the full pool ranked each texture by its least important request and loaded detail only in one piece, both fixed and owner verified in a thirty car race, AI cars and some props still blurry in a full field by the owner's call
- [BUG-003-menu-icons-stop-rendering](BUG-003-menu-icons-stop-rendering.md), VRAM starvation by the staging buffers, fixed by the 128 MB cap
- [BUG-004-crash-on-car-or-track-change](BUG-004-crash-on-car-or-track-change.md), VRAM exhaustion on scene switch, same fix, owner verified
- [BUG-005-crash-on-startup](BUG-005-crash-on-startup.md), VRAM exhaustion on first load, same fix, owner verified
- [BUG-007-blurry-road-and-textures](BUG-007-blurry-road-and-textures.md), small race pool plus texture quality High, fixed by the fixed pool and Ultra
- [BUG-010-texture-pool-shrinks-on-race-load-and-restart](BUG-010-texture-pool-shrinks-on-race-load-and-restart.md), pool sized during scene transitions, fixed by fixed pool sizes, owner verified
