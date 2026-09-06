---
name: todos-index
kind: doc
description: the work tracker's index, open items first
updated: 2026-09-06
links: [agent-index, spec-todos]
---

# Todos Index

## Open

engine-flags

- [TODO-012-unlock-the-free-roam-mode](TODO-012-unlock-the-free-roam-mode.md), the engine's Free Roam mode sits behind a password switch, unlock it from the mod and get the owner driving the roads around the Nordschleife

streaming

- [TODO-013-faster-session-loads](TODO-013-faster-session-loads.md), the 15 s Nordschleife load with the disk mostly idle: sample the loading workers, then a read set cache or the decipher off the game
- [TODO-009-overlay-serves-copies-so-loose-files-stay-editable](TODO-009-overlay-serves-copies-so-loose-files-stay-editable.md), serve cached copies so loose files are not locked while the game runs

render

- [TODO-010-resume-the-one-percent-low-hunt](TODO-010-resume-the-one-percent-low-hunt.md), the parked 1 percent low hunt: the leads never tested and the instruments to bring back

## Done

- render: 1 (TODO-002, the optimisation pass: exceptions, the tiled instances buffer, BypassIO and the thread pools all checked and clean)
- streaming: 3 (TODO-001, TODO-005, TODO-007 the package override layer, replace and add verified)
- tooling: 1 (TODO-006)
- release: 1 (TODO-004)

## Dropped

- stability: 1 (TODO-003, the crashes stopped with the staging cap)
- ui: 1 (TODO-011, the UI script overhaul, one round tried and closed on the owner's word)
