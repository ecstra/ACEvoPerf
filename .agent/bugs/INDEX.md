---
name: bugs-index
kind: doc
description: the defect tracker's index, open bugs first
updated: 2026-09-24
links: [agent-index, spec-bugs]
---

# Bugs Index

## Open

breaks

- [BUG-037-crash-joining-a-session-with-custom-track-mods-installed](BUG-037-crash-joining-a-session-with-custom-track-mods-installed.md), a player on reddit reports a crash joining a session with custom track mods installed under 0.3.2 that the stock files do not have, not reproduced, the likeliest lead the mod's smaller loading buffer failing a bigger custom track request

bug

- [BUG-032-the-game-freezes-at-a-thirty-ai-race-start](BUG-032-the-game-freezes-at-a-thirty-ai-race-start.md), the game stopped presenting at the start of a thirty AI race at the Nürburgring with its own threads frozen for 45 s while the mod's kept running, no exception and video memory over budget, not reproduced since
- [BUG-036-textures-resolve-in-visible-steps-and-a-mid-lap-restart-makes-it-worse](BUG-036-textures-resolve-in-visible-steps-and-a-mid-lap-restart-makes-it-worse.md), the scene comes up mushy and sharpens in several visible stages rather than in one go, and a session restart from mid lap makes the staircase worse or slower than the same scene loaded fresh
- [BUG-035-the-writing-on-the-ground-is-pixelated](BUG-035-the-writing-on-the-ground-is-pixelated.md), the painted and chalked writing on the track surface is blocky up close while the tarmac under it is sharp, so the layer carrying the writing sits at a lower detail level than the surface it is on
- [BUG-034-an-integrated-gpu-with-a-large-uma-carve-out-is-read-as-a-card-that-size](BUG-034-an-integrated-gpu-with-a-large-uma-carve-out-is-read-as-a-card-that-size.md), the auto sizes rank adapters by dedicated video memory and cannot tell an integrated GPU from a discrete one before a device exists, so an APU with a 4 or 8 GB firmware carve out takes a card's bracket and can outrank a smaller real card beside it, accepted as a limit in DEC-022
- [BUG-033-a-menu-view-remade-at-a-new-size-is-not-recognised](BUG-033-a-menu-view-remade-at-a-new-size-is-not-recognised.md), the menu and HUD view is told by being first or by matching the first view's size, so one remade at a different size, which a fullscreen to windowed change does, loses the page fixes for the rest of the session, the accepted residue of F-02 of the cohtml build guard review
- [BUG-018-whole-scene-low-detail-for-a-second-after-load](BUG-018-whole-scene-low-detail-for-a-second-after-load.md), the whole scene is coarse for a second or two after the curtain lifts, at start-up and at track entry, distinct from BUG-001, measurement armed
- [BUG-019-car-physics-rebuilds-every-tyre-model-five-times](BUG-019-car-physics-rebuilds-every-tyre-model-five-times.md), the 296 GT3's twenty tyre compound builds span 3.52 s on the serial chain that ends the session load, but each build takes about 0.15 ms and the time sits around the compound asset fetch between them
- [BUG-002-fps-drop-entering-new-track-sections](BUG-002-fps-drop-entering-new-track-sections.md), GPU pinned and thermally throttled during the lap
- [BUG-009-one-percent-lows-far-below-average](BUG-009-one-percent-lows-far-below-average.md), the same gap on the owner's 5070 desktop with no integrated GPU rules out the integrated GPU present path, the UI view rotation is about 30 percent of the gap at the Red Bull Ring GP and the HUD every frame removes it at no cost, no refresh hold on this laptop even at 60 Hz, the fix driven and holding, PresentMon agrees with the NVIDIA overlay and shows a clean independent flip path, the trace of a good launch spreads the heavy frames over GPU waits, a compositor released frame queue, one heavy stretch of the lap and small engine costs, the bad launch not yet traced
- [BUG-013-one-percent-lows-drop-after-window-or-input-switch](BUG-013-one-percent-lows-drop-after-window-or-input-switch.md), pause and HUD reload stalls through a rolling counter plus the device rebuild on a device change, diagnostics removed, the device stays to be named
- [BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load](BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load.md), 1.2 MB of UI stylesheets read and parsed again at every document load, 38 MB of repeat reads in a 20 minute race session, the parse off the frame thread since the responsive UI

debt

- [BUG-023-overlay-keeps-a-64-mb-table-copy-for-the-whole-session](BUG-023-overlay-keeps-a-64-mb-table-copy-for-the-whole-session.md), the override layer holds a 64 MB decoded package table for the whole session though the game reads its table in the first seconds

nit

- [BUG-031-the-heap-still-grows-about-5-mb-a-visit-with-the-session-leak-fix](BUG-031-the-heap-still-grows-about-5-mb-a-visit-with-the-session-leak-fix.md), with every finished session freed the live heap still grows about 5 MB a Red Bull Ring visit, the remaining leak in the owner's words, not named yet
- [BUG-030-menus-drop-to-50-to-60-fps-while-moving-things-quickly](BUG-030-menus-drop-to-50-to-60-fps-while-moving-things-quickly.md), moving something quickly in the static menus drops to 50 to 60 fps with the 1 percent low at times 22, the probe caught the vehicle setup page restyling all 1,161 nodes six times a second through an attribute change on one component, the attribute and the owner's movement to be named
- [BUG-028-page-opens-still-hold-frames-of-100-to-200-ms](BUG-028-page-opens-still-hold-frames-of-100-to-200-ms.md), with the responsive UI on the heavier pages still open with a frame or two of 100 to 200 ms, the page's own script, pages built over several frames restyling much of the page, the listed rules still walked
- [BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring](BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring.md), every camera cut of the Red Bull Ring pit menu starts blurry, a cut forces a streamer pass that loads before it drops, often cannot load, then waits a second, the follow up pass of TODO-024 is the correction to test, and on 2026-09-16 the owner saw a pit wall poster sharpen, blur and come back less sharp, a trace to confirm

## Won't fix

- [BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache](BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache.md), unlit trees at night reported with `enable_pso_cache` on, the flag was shipped off for a while, back on at the owner's word with nothing on the reference machine showing the fault, a user log reopens it
- [BUG-012-pit-lane-return-freezes-over-a-second](BUG-012-pit-lane-return-freezes-over-a-second.md), the 1.2 s is the engine parsing its 63 MB zlib and protobuf track preset at every session start and restart, same on every card
- [BUG-001-texture-low-mip-shown-before-streaming](BUG-001-texture-low-mip-shown-before-streaming.md), the mip a surface gets at a distance is the engine's choice on every card, the streaming path answers in 9 ms bursts
- [BUG-006-distant-objects-pop-in](BUG-006-distant-objects-pop-in.md), grass fades at 30 m and trees switch at 100 m by the package's own values on every card, the Custom LOD setting moves them at a cost

## Fixed

- [BUG-016-vram-overhead-grows-across-scene-loads](BUG-016-vram-overhead-grows-across-scene-loads.md), the game's memory grew with every scene load, named by the census run as every finished session staying whole in memory behind a cycle between its local server connection and its game mode, fixed by the session leak fix over a six visit run and the owner's own play, what is left of the growth is BUG-031
- [BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash](BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash.md), the streamer log line read the engine's freed allocator at exit and the game logged a crash report naming the mod, fixed by printing the pool figures the last kick read, a clean quit after the census run's fifteen loads
- [BUG-029-the-hud-restyles-most-of-its-page-while-driving](BUG-029-the-hud-restyles-most-of-its-page-while-driving.md), the HUD restyled its whole page for 21 to 24 ms when a part left its top level, the wrong way label's data-bind-if and Cohtml's child removal set that matches every node, fixed by the responsive UI's child removal fix, owner driven with five wrong way episodes and no restyle over 15 ms while driving
- [BUG-014-ui-pages-lag-on-open-switch-and-interaction](BUG-014-ui-pages-lag-on-open-switch-and-interaction.md), the menus lagged on hover, scrolling, sliders, switching and opening pages, fixed by the responsive UI over seven owner driven laps, what page opens still cost is BUG-028
- [BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three](BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three.md), the pit menu and its pages advanced the UI one frame in three, fixed by keeping a menu page's surface in every frame while the car displays take turns, owner verified
- [BUG-025-controls-page-scans-the-page-once-per-new-row](BUG-025-controls-page-scans-the-page-once-per-new-row.md), each new controls row scanned the whole page once per navigation section, fixed by folding the calls into one scan a frame, owner driven
- [BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open](BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open.md), vehicle setup requested and built its setup twice per open, fixed by ignoring the second init while the first is out, owner driven
- [BUG-017-trackside-big-screens-blurry](BUG-017-trackside-big-screens-blurry.md), the big screen flipbook ships with 3 of 12 mip levels on an 8 by 8 grid, fixed by the overlay serving it with one mip level
- [BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles](BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles.md), the full pool ranked each texture by its least important request and loaded detail only in one piece, both fixed and owner verified in a thirty car race, AI cars and some props still blurry in a full field by the owner's call
- [BUG-003-menu-icons-stop-rendering](BUG-003-menu-icons-stop-rendering.md), VRAM starvation by the staging buffers, fixed by the 128 MB cap
- [BUG-004-crash-on-car-or-track-change](BUG-004-crash-on-car-or-track-change.md), VRAM exhaustion on scene switch, same fix, owner verified
- [BUG-005-crash-on-startup](BUG-005-crash-on-startup.md), VRAM exhaustion on first load, same fix, owner verified
- [BUG-007-blurry-road-and-textures](BUG-007-blurry-road-and-textures.md), small race pool plus texture quality High, fixed by the fixed pool and Ultra
- [BUG-010-texture-pool-shrinks-on-race-load-and-restart](BUG-010-texture-pool-shrinks-on-race-load-and-restart.md), pool sized during scene transitions, fixed by fixed pool sizes, owner verified
