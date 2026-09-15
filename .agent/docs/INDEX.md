---
name: docs-index
kind: doc
description: index of the knowledge library, one line per doc
updated: 2026-09-15
links: [agent-index, spec-docs]
---

# Docs Index

## foundation

- [proxy-architecture](foundation/proxy-architecture.md), what the DLL does in load order and where each piece lives

## systems

- [directstorage-streaming](systems/directstorage-streaming.md), the three queues, the texture streamer's kick, the staging buffer and how the VRAM pools are sized
- [engine-flags](systems/engine-flags.md), the 216 gflags, which matter, how the mod sets them
- [content-package](systems/content-package.md), content.kspkg layout, hash and cipher
- [package-override-layer](systems/package-override-layer.md), loose files under acevo_mods shadow package entries, how and what was verified
- [settings-files](systems/settings-files.md), user data location, which files are the account, the controls and the graphics, and the VideoSettings fields worth knowing
- [responsive-ui](systems/responsive-ui.md), the one switch that keeps the menus smooth, its six parts, what each patches in Cohtml, the exe or the pages and how it checks the build, and the shared Cohtml hooks

## ops

- [telemetry](ops/telemetry.md), the log and CSV files and their columns, the streaming trace, the UI probe, the game log's crash logger, plus the GPU sampler
- [build-and-release](ops/build-and-release.md), build, install, uninstall, gates, and publishing on Overtake and GitHub
- [tools](ops/tools.md), the Python tools and their commands
- [public-docs](ops/public-docs.md), how the readme, the changelog and the zip readme are written for players, and how the next version is tracked

## research

- [moddability](research/moddability.md), how the game is built and what a mod can change
- [lap-2026-09-05-nordschleife](research/lap-2026-09-05-nordschleife.md), the first telemetry lap and what it showed
- [one-percent-low-hunt-2026-09-05](research/one-percent-low-hunt-2026-09-05.md), nineteen instrumented laps into the 1 percent low, what was measured, ruled out and left, read again by the 2026-09-14 deep dive
- [ui-lag-hunt-2026-09-06](research/ui-lag-hunt-2026-09-06.md), a morning of instrumented runs into the menu and settings page lag, every DLL lever measured, the engine's surface, with the readings the 2026-09-14 deep dive corrected
- [ui-lag-deepdive-2026-09-14](research/ui-lag-deepdive-2026-09-14.md), BUG-014's deep dive, the UI lag split into five costs, the controls page freeze as navigation scans in script, interaction as one Cohtml task per update, the pit menu updated one frame in three, document loads, a light HUD, twelve ranked fixes and one decisive developer build
- [responsive-ui-rounds-2026-09-15](research/responsive-ui-rounds-2026-09-15.md), the seven probe laps that fixed BUG-014, generic hover selectors restyling whole pages, menus at a third of the frame rate, a slider refresh storm, style matching and the frame thread's stylesheet parses at page opens, the probe's own stalls, numbers before and after and what page opens still cost
- [free-roam-unlock-2026-09-06](research/free-roam-unlock-2026-09-06.md), the hidden Free Roam mode found behind a password, switched on for an afternoon and dropped: the gate, what the package carries and lacks, the recipe
- [ghost-car-2026-09-06](research/ghost-car-2026-09-06.md), the ghost car flag records, saves, loads and samples a lap every frame and nothing draws it, two sessions and the disassembly, dropped
- [reflex-2026-09-12](research/reflex-2026-09-12.md), Reflex added to a game that ships none, confirmed by the driver, and four two lap runs showing why a card pinned at its thermal limit cannot show a difference in rate, read again as the part of the mod that evens the frame pacing
- [directstorage-1-3-2026-09-12](research/directstorage-1-3-2026-09-12.md), the runtime taken to 1.3.0, what the forwarder and core split really is, the attempt that shipped and silently did nothing, and the measurements showing the upgrade changed nothing
- [optimisation-deepdive-2026-09-12](research/optimisation-deepdive-2026-09-12.md), eighteen agents over eight angles outside streaming, then every kill re-verified by hand, 24 killed for good and 9 back to unresolved of which DLSS then closed, with the sampler census five verdicts leaned on shown not to exist
- [engine-flags-in-game-2026-09-12](research/engine-flags-in-game-2026-09-12.md), three parked runs on the never used in-game flags: global illumination costs 3.2 percent and 416 MB and neither half is reachable, the probe count is dead, the PSO log line does not exist in the release build, and the machine loses 8.3 percent standing still
- [tile-pool-reshuffle-2026-09-12](research/tile-pool-reshuffle-2026-09-12.md), the parked tile churn as first measured, 22 MB/s and about 4 percent of frame time, with the probe and dedupe of that day and the readings the next round corrected
- [frame-time-mod-against-passive-2026-09-13](research/frame-time-mod-against-passive-2026-09-13.md), the mod costs 3.8 percent parked and 8 percent on a lap against itself passive on a GPU held still, most of it mesh detail the 1433 MB mesh budget loads, which 366 MB gets back unseen but starves the mesh streamer, frozen on the GP and churning at the Red Bull Ring, with the corrections of the mesh deep dive
- [texture-streamer-flip-2026-09-13](research/texture-streamer-flip-2026-09-13.md), the churn is the texture streamer reading feedback measured against the loaded mip as if against the full texture, confirmed live and fixed behind an off by default switch, plus the full pool, the Red Bull Ring's driving traffic and 3.5 GB of repeated reads
- [texture-streamer-overload-2026-09-13](research/texture-streamer-overload-2026-09-13.md), a census of every kick in a thirty car race shows the streamer ranking each texture by its least important request and loading only whole steps, both fixed and seen fixed in game, with the player's car and driver holding half the 1 GB pool and AI cars ranked like props, left that way on the owner's call
- [mesh-level-of-detail-2026-09-14](research/mesh-level-of-detail-2026-09-14.md), TODO-022's deep dive, how the mesh budget is sized, how the mesh streamer loads and how the renderer picks the level it draws, the 1433 MB budget's frame cost as authored detail and not an engine fault, the `-log_info=meshStreamer` readout and a toggle plan for the last 1 percent
- [one-percent-lows-2026-09-14](research/one-percent-lows-2026-09-14.md), BUG-009's deep dive, the slowest frames are the present path coupled to the previous frame's GPU end, spread out renderer code and a ripple from the game updating one UI view per frame, not a lock or a job, Reflex already evening the alternation, no processor shortage, and two runs with no build
- [texture-streamer-camera-cuts-2026-09-14](research/texture-streamer-camera-cuts-2026-09-14.md), BUG-021's deep dive, a camera cut forces a streamer pass that loads before it drops, starts at most 128 loads and none while other resource jobs run, then waits a second, the car dropped whole and the scenery three passes behind, the shipped fixes changing none of it, and a follow up pass as the correction
- [memory-creep-2026-09-14](research/memory-creep-2026-09-14.md), BUG-016's deep dive, the census read with VRAM taken out, a one time heap fill then about 110 MB a track the game keeps, every heap lever from a DLL measured and not worth shipping, the VRAM overhead spike as placement, page file space the only cost
