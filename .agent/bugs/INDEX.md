---
name: bugs-index
kind: doc
description: the defect tracker's index, open bugs first
updated: 2026-09-15
links: [agent-index, spec-bugs]
---

# Bugs Index

## Open

bug

- [BUG-016-vram-overhead-grows-across-scene-loads](BUG-016-vram-overhead-grows-across-scene-loads.md), a one time heap fill of about 1.25 GB with the first track and then about 110 MB a track the game keeps, the same passive, page file space the only cost found, the VRAM overhead spike is placement, what grows is unnamed until TODO-023's run
- [BUG-018-whole-scene-low-detail-for-a-second-after-load](BUG-018-whole-scene-low-detail-for-a-second-after-load.md), the whole scene is coarse for a second or two after the curtain lifts, at start-up and at track entry, distinct from BUG-001, measurement armed
- [BUG-019-car-physics-rebuilds-every-tyre-model-five-times](BUG-019-car-physics-rebuilds-every-tyre-model-five-times.md), the 296 GT3's twenty tyre compound builds span 3.52 s on the serial chain that ends the session load, but each build takes about 0.15 ms and the time sits around the compound asset fetch between them
- [BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash](BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash.md), the streamer log line reads the engine's freed allocator at exit, the mod catches it but the game writes a crash report naming the mod
- [BUG-002-fps-drop-entering-new-track-sections](BUG-002-fps-drop-entering-new-track-sections.md), GPU pinned and thermally throttled during the lap
- [BUG-009-one-percent-lows-far-below-average](BUG-009-one-percent-lows-far-below-average.md), the slowest frames are the present path through the integrated GPU coupled to the previous frame's GPU end, spread out renderer code and a ripple from one UI view updated per frame, not a lock or a job, Reflex already evens the alternation, two runs with no build next
- [BUG-013-one-percent-lows-drop-after-window-or-input-switch](BUG-013-one-percent-lows-drop-after-window-or-input-switch.md), pause and HUD reload stalls through a rolling counter plus the device rebuild on a device change, diagnostics removed, the device stays to be named
- [BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load](BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load.md), 1.2 MB of UI stylesheets read and parsed again at every document load, 38 MB of repeat reads in a 20 minute race session, the parse off the frame thread since the responsive UI

debt

- [BUG-023-overlay-keeps-a-64-mb-table-copy-for-the-whole-session](BUG-023-overlay-keeps-a-64-mb-table-copy-for-the-whole-session.md), the override layer holds a 64 MB decoded package table for the whole session though the game reads its table in the first seconds

nit

- [BUG-028-page-opens-still-hold-frames-of-100-to-200-ms](BUG-028-page-opens-still-hold-frames-of-100-to-200-ms.md), with the responsive UI on the heavier pages still open with a frame or two of 100 to 200 ms, the page's own script, pages built over several frames restyling much of the page, the listed rules still walked
- [BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring](BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring.md), every camera cut of the Red Bull Ring pit menu starts blurry, a cut forces a streamer pass that loads before it drops, often cannot load, then waits a second, the follow up pass of TODO-024 is the correction to test

## Won't fix

- [BUG-012-pit-lane-return-freezes-over-a-second](BUG-012-pit-lane-return-freezes-over-a-second.md), the 1.2 s is the engine parsing its 63 MB zlib and protobuf track preset at every session start and restart, same on every card
- [BUG-001-texture-low-mip-shown-before-streaming](BUG-001-texture-low-mip-shown-before-streaming.md), the mip a surface gets at a distance is the engine's choice on every card, the streaming path answers in 9 ms bursts
- [BUG-006-distant-objects-pop-in](BUG-006-distant-objects-pop-in.md), grass fades at 30 m and trees switch at 100 m by the package's own values on every card, the Custom LOD setting moves them at a cost

## Fixed

- [BUG-014-ui-pages-lag-on-open-switch-and-interaction](BUG-014-ui-pages-lag-on-open-switch-and-interaction.md), the menus lagged on hover, scrolling, sliders, switching and opening pages, fixed by the responsive UI over seven owner driven laps, what page opens still cost is BUG-028
- [BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three](BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three.md), the pit menu and its pages advanced the UI one frame in three, fixed by keeping a menu page's surface in every frame while the car displays take turns, owner verified
- [BUG-025-controls-page-scans-the-page-once-per-new-row](BUG-025-controls-page-scans-the-page-once-per-new-row.md), each new controls row scanned the whole page once per navigation section, fixed by folding the calls into one scan a frame, owner driven
- [BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open](BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open.md), vehicle setup requested and built its setup twice per open, fixed by ignoring the second init while the first is out, owner driven
- [BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache](BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache.md), unlit trees at night reported with `enable_pso_cache` on, the flag shipped off and the owner saw night lighting work, closed on the owner's word with the mechanism still unexplained
- [BUG-017-trackside-big-screens-blurry](BUG-017-trackside-big-screens-blurry.md), the big screen flipbook ships with 3 of 12 mip levels on an 8 by 8 grid, fixed by the overlay serving it with one mip level
- [BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles](BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles.md), the full pool ranked each texture by its least important request and loaded detail only in one piece, both fixed and owner verified in a thirty car race, AI cars and some props still blurry in a full field by the owner's call
- [BUG-003-menu-icons-stop-rendering](BUG-003-menu-icons-stop-rendering.md), VRAM starvation by the staging buffers, fixed by the 128 MB cap
- [BUG-004-crash-on-car-or-track-change](BUG-004-crash-on-car-or-track-change.md), VRAM exhaustion on scene switch, same fix, owner verified
- [BUG-005-crash-on-startup](BUG-005-crash-on-startup.md), VRAM exhaustion on first load, same fix, owner verified
- [BUG-007-blurry-road-and-textures](BUG-007-blurry-road-and-textures.md), small race pool plus texture quality High, fixed by the fixed pool and Ultra
- [BUG-010-texture-pool-shrinks-on-race-load-and-restart](BUG-010-texture-pool-shrinks-on-race-load-and-restart.md), pool sized during scene transitions, fixed by fixed pool sizes, owner verified
