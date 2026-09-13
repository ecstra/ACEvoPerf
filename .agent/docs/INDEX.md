---
name: docs-index
kind: doc
description: index of the knowledge library, one line per doc
updated: 2026-09-13
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

## ops

- [telemetry](ops/telemetry.md), the log and CSV files and their columns, the streaming trace, plus the GPU sampler
- [build-and-release](ops/build-and-release.md), build, install, uninstall, gates, and publishing on Overtake and GitHub
- [tools](ops/tools.md), the Python tools and their commands

## research

- [moddability](research/moddability.md), how the game is built and what a mod can change
- [lap-2026-09-05-nordschleife](research/lap-2026-09-05-nordschleife.md), the first telemetry lap and what it showed
- [one-percent-low-hunt-2026-09-05](research/one-percent-low-hunt-2026-09-05.md), nineteen instrumented laps into the 1 percent low, what was measured, ruled out and left
- [ui-lag-hunt-2026-09-06](research/ui-lag-hunt-2026-09-06.md), a morning of instrumented runs into the menu and settings page lag, every DLL lever measured, where the cost sits, the engine's surface, why the mod left the UI alone
- [free-roam-unlock-2026-09-06](research/free-roam-unlock-2026-09-06.md), the hidden Free Roam mode found behind a password, switched on for an afternoon and dropped: the gate, what the package carries and lacks, the recipe
- [ghost-car-2026-09-06](research/ghost-car-2026-09-06.md), the ghost car flag records, saves, loads and samples a lap every frame and nothing draws it, two sessions and the disassembly, dropped
- [reflex-2026-09-12](research/reflex-2026-09-12.md), Reflex added to a game that ships none, confirmed by the driver, and four two lap runs showing why a card pinned at its thermal limit cannot show a difference
- [directstorage-1-3-2026-09-12](research/directstorage-1-3-2026-09-12.md), the runtime taken to 1.3.0, what the forwarder and core split really is, the attempt that shipped and silently did nothing, and the measurements showing the upgrade changed nothing
- [optimisation-deepdive-2026-09-12](research/optimisation-deepdive-2026-09-12.md), eighteen agents over eight angles outside streaming, then every kill re-verified by hand, 24 killed for good and 9 back to unresolved of which DLSS then closed, with the sampler census five verdicts leaned on shown not to exist
- [engine-flags-in-game-2026-09-12](research/engine-flags-in-game-2026-09-12.md), three parked runs on the never used in-game flags: global illumination costs 3.2 percent and 416 MB and neither half is reachable, the probe count is dead, the PSO log line does not exist in the release build, and the machine loses 8.3 percent standing still
- [tile-pool-reshuffle-2026-09-12](research/tile-pool-reshuffle-2026-09-12.md), the parked tile churn as first measured, 22 MB/s and about 4 percent of frame time, with the probe and dedupe of that day and the readings the next round corrected
- [frame-time-mod-against-passive-2026-09-13](research/frame-time-mod-against-passive-2026-09-13.md), the mod costs 3.8 percent parked and 8 percent on a lap against itself passive on a GPU held still, three quarters of it the bigger budgets, of which the 1433 MB mesh budget costs 2 percent parked and 5 on a lap for a small part of the picture while the texture pool buys most of it for under 1, the reload fix ruled out
- [texture-streamer-flip-2026-09-13](research/texture-streamer-flip-2026-09-13.md), the churn is the texture streamer reading feedback measured against the loaded mip as if against the full texture, confirmed live and fixed behind an off by default switch, plus the full pool, the Red Bull Ring's driving traffic and 3.5 GB of repeated reads
