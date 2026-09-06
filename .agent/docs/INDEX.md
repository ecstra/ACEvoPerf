---
name: docs-index
kind: doc
description: index of the knowledge library, one line per doc
updated: 2026-09-06
links: [agent-index, spec-docs]
---

# Docs Index

## foundation

- [proxy-architecture](foundation/proxy-architecture.md), what the DLL does in load order and where each piece lives

## systems

- [directstorage-streaming](systems/directstorage-streaming.md), the three queues, the staging buffer and how the VRAM pools are sized
- [engine-flags](systems/engine-flags.md), the 216 gflags, which matter, how the mod sets them
- [content-package](systems/content-package.md), content.kspkg layout, hash and cipher
- [package-override-layer](systems/package-override-layer.md), loose files under acevo_mods shadow package entries, how and what was verified
- [settings-files](systems/settings-files.md), user data location and the VideoSettings fields worth knowing
- [cohtml-ui-engine](systems/cohtml-ui-engine.md), the Coherent Gameface engine behind the menus, what the proxy reaches in it, the inspector, how the pages and scripts behave

## ops

- [telemetry](ops/telemetry.md), the log and CSV files and their columns, plus the GPU sampler
- [build-and-release](ops/build-and-release.md), build, install, uninstall, gates
- [tools](ops/tools.md), the Python tools and their commands

## research

- [moddability](research/moddability.md), how the game is built and what a mod can change
- [lap-2026-09-05-nordschleife](research/lap-2026-09-05-nordschleife.md), the first telemetry lap and what it showed
- [one-percent-low-hunt-2026-09-05](research/one-percent-low-hunt-2026-09-05.md), nineteen instrumented laps into the 1 percent low, what was measured, ruled out and left
- [ui-lag-hunt-2026-09-06](research/ui-lag-hunt-2026-09-06.md), a morning of instrumented runs into the menu and settings page lag, every DLL lever measured, where the cost sits, what a fix takes
