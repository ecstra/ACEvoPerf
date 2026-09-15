---
name: todos-index
kind: doc
description: the work tracker's index, open items first
updated: 2026-09-15
links: [agent-index, spec-todos]
---

# Todos Index

## Open

release

- [TODO-020-cut-0-3-2-after-the-three-open-investigations](TODO-020-cut-0-3-2-after-the-three-open-investigations.md), 0.3.2 waits for TODO-018, BUG-009 and BUG-016 to each have their round, because the release is meant to be final rather than a staging post, TODO-018 done and the other two past their deep dives with runs to come

streaming

- [TODO-013-faster-session-loads](TODO-013-faster-session-loads.md), 17 s on the Nurburgring, three quarters of it one streaming phase, the drive ruled out and readahead built and thrown away, next is the sampler on the resource workers
- [TODO-009-overlay-serves-copies-so-loose-files-stay-editable](TODO-009-overlay-serves-copies-so-loose-files-stay-editable.md), serve cached copies so loose files are not locked while the game runs
- [TODO-014-proxy-implements-enqueuerequests-if-the-game-asks](TODO-014-proxy-implements-enqueuerequests-if-the-game-asks.md), the proxy declines IDStorageQueue3 to keep the overlay and the statistics in the path, implement it if a game build ever asks
- [TODO-019-tile-upload-dedupe-done-properly](TODO-019-tile-upload-dedupe-done-properly.md), the redundant tile traffic left now the parked churn is fixed at its source, driving, 97 percent of 20 GB in ten minutes at the Red Bull Ring, starting from the recorded trace rather than the old dedupe
- [TODO-024-a-follow-up-streamer-pass-after-a-camera-cut](TODO-024-a-follow-up-streamer-pass-after-a-camera-cut.md), BUG-021's first correction, a streamer pass fired when the Resource Manager's job count reads zero after a cut whose pass could not load, tested behind an every other cut arm in one pit menu run
- [TODO-021-the-engine-reads-the-same-data-twice](TODO-021-the-engine-reads-the-same-data-twice.md), 3.5 GB of package data read a second time in one session, 633 MB of it inside one Nürburgring load, operation rather than load time in the owner's words

render

- [TODO-010-resume-the-one-percent-low-hunt](TODO-010-resume-the-one-percent-low-hunt.md), the 1 percent low hunt through the 2026-09-14 deep dive, its leads answered or moved to two runs, the done-when waiting on the owner's choice of measure
- [TODO-025-the-ui-view-rotation-test](TODO-025-the-ui-view-rotation-test.md), two launches with no build stepping the dashboard displays setting and turning the dashboards off, to prove the UI view rotation's ripple and name the heavy view
- [TODO-022-frame-time-with-and-without-the-mod](TODO-022-frame-time-with-and-without-the-mod.md), the mod's 3.8 percent parked is the mesh detail the 1433 MB budget loads, authored detail per the mesh deep dive, the engine's own readout to ride a planned run, the last 1 percent unsplit
- [TODO-017-tier-2-variable-rate-shading](TODO-017-tier-2-variable-rate-shading.md), built and measured at 4x4, the ceiling: no frames on a thermally pinned card, stage two dropped, and the 3 percent it appeared to gain was the streamer churn it suppressed by starving feedback

tooling

- [TODO-026-one-lean-etw-trace-of-the-slow-frames](TODO-026-one-lean-etw-trace-of-the-slow-frames.md), one Windows performance trace started from the owner's elevated prompt, naming what the present thread waits on and who wakes it in the slowest frames

stability

- [TODO-023-name-what-the-game-keeps-across-identical-loads](TODO-023-name-what-the-game-keeps-across-identical-loads.md), the memory census back, six identical Nürburgring GP visits and two memory dumps, to tell BUG-016's fill from a leak and name what grows

engine-flags

- [TODO-016-run-the-four-untouched-in-game-engine-flags](TODO-016-run-the-four-untouched-in-game-engine-flags.md), `no_gi`, `log_pso_on_creation`, `ai_run_dynamic_track` and `car_update_complete_max_interval` reachable from the ini today with no build, measured from a fixed stationary view

## Done

- render: 1 (TODO-002, the optimisation pass: exceptions, the tiled instances buffer, BypassIO and the thread pools all checked and clean)
- streaming: 5 (TODO-001, TODO-005, TODO-007 the package override layer, replace and add verified; TODO-015, the game's texture requests cannot merge, zero of 32201, measured and closed; TODO-018, the streaming layer looked at directly, the churn traced to the streamer's feedback flip and fixed)
- tooling: 1 (TODO-006)
- release: 1 (TODO-004)
- ui: 1 (TODO-027, the UI developer build became the UI probe and seven laps that fixed BUG-014 as the responsive UI)

## Dropped

- stability: 1 (TODO-003, the crashes stopped with the staging cap)
- ui: 1 (TODO-011, the UI script overhaul, one round tried and closed on the owner's word)
- engine-flags: 1 (TODO-012, the hidden Free Roam mode unlocked and driven for an afternoon, dropped because the package stops at the complex, DEC-011)
