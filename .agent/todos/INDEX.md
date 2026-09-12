---
name: todos-index
kind: doc
description: the work tracker's index, open items first
updated: 2026-09-12
links: [agent-index, spec-todos]
---

# Todos Index

## Open

release

- [TODO-020-cut-0-3-2-after-the-three-open-investigations](TODO-020-cut-0-3-2-after-the-three-open-investigations.md), 0.3.2 waits for TODO-018, BUG-009 and BUG-016 to each have their round, because the release is meant to be final rather than a staging post

streaming

- [TODO-013-faster-session-loads](TODO-013-faster-session-loads.md), 17 s on the Nurburgring, three quarters of it one streaming phase, the drive ruled out and readahead built and thrown away, next is the sampler on the resource workers
- [TODO-009-overlay-serves-copies-so-loose-files-stay-editable](TODO-009-overlay-serves-copies-so-loose-files-stay-editable.md), serve cached copies so loose files are not locked while the game runs
- [TODO-014-proxy-implements-enqueuerequests-if-the-game-asks](TODO-014-proxy-implements-enqueuerequests-if-the-game-asks.md), the proxy declines IDStorageQueue3 to keep the overlay and the statistics in the path, implement it if a game build ever asks
- [TODO-018-look-properly-at-the-streaming-layer](TODO-018-look-properly-at-the-streaming-layer.md), a 22 MB/s re-read loop sat in the streaming layer for the life of the project and only surfaced by accident, because every instrument pointed at it has been an aggregate
- [TODO-019-tile-upload-dedupe-done-properly](TODO-019-tile-upload-dedupe-done-properly.md), it works and removes 1.4 GB of disk and upload traffic per lap, dropped only because its free list runs dry, so the round starts with the engine's eviction signal

render

- [TODO-010-resume-the-one-percent-low-hunt](TODO-010-resume-the-one-percent-low-hunt.md), the parked 1 percent low hunt: the leads never tested and the instruments to bring back
- [TODO-017-tier-2-variable-rate-shading](TODO-017-tier-2-variable-rate-shading.md), built and measured at 4x4, the ceiling: no frames on a thermally pinned card, stage two dropped, and the 3 percent it appeared to gain was the tile pool churn it suppressed

engine-flags

- [TODO-016-run-the-four-untouched-in-game-engine-flags](TODO-016-run-the-four-untouched-in-game-engine-flags.md), `no_gi`, `log_pso_on_creation`, `ai_run_dynamic_track` and `car_update_complete_max_interval` reachable from the ini today with no build, measured from a fixed stationary view

## Done

- render: 1 (TODO-002, the optimisation pass: exceptions, the tiled instances buffer, BypassIO and the thread pools all checked and clean)
- streaming: 4 (TODO-001, TODO-005, TODO-007 the package override layer, replace and add verified; TODO-015, the game's texture requests cannot merge, zero of 32201, measured and closed)
- tooling: 1 (TODO-006)
- release: 1 (TODO-004)

## Dropped

- stability: 1 (TODO-003, the crashes stopped with the staging cap)
- ui: 1 (TODO-011, the UI script overhaul, one round tried and closed on the owner's word)
- engine-flags: 1 (TODO-012, the hidden Free Roam mode unlocked and driven for an afternoon, dropped because the package stops at the complex, DEC-011)
