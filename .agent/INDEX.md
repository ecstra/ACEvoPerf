---
name: agent-index
kind: doc
description: the spine of the agent directory, what exists and where
updated: 2026-10-08
links: [agent-readme, conventions, house-rules-agent]
---

# Index

The spine. Read this first, then follow into the folder indexes. The
upkeep rule (spec/conventions.md): every file change updates its index
line in the same commit.

## The directory itself

- [README.md](README.md), what this directory is and how to boot from it
- [house-rules.md](house-rules.md), the branch contract and review protocol, the operating version
- [spec/conventions.md](spec/conventions.md), the shared law: frontmatter, naming, links, indexes
- [spec/docs.md](spec/docs.md), format and folder rules for the knowledge library
- [spec/memory.md](spec/memory.md), format and boundaries for project memory
- [spec/bugs.md](spec/bugs.md), the defect tracker's format and lifecycle
- [spec/todos.md](spec/todos.md), the work tracker's format, intake, and lifecycle
- [spec/decisions.md](spec/decisions.md), one decision per file, how
- [spec/reviews.md](spec/reviews.md), the review ledger format and lifecycle
- [spec/handover.md](spec/handover.md), session handoff notes, how

## The knowledge

- [docs/INDEX.md](docs/INDEX.md), the knowledge library: 34 docs in four categories
  - foundation: [proxy-architecture](docs/foundation/proxy-architecture.md), the DLL in load order
  - systems: [directstorage-streaming](docs/systems/directstorage-streaming.md), [engine-flags](docs/systems/engine-flags.md), [content-package](docs/systems/content-package.md), [package-override-layer](docs/systems/package-override-layer.md), [settings-files](docs/systems/settings-files.md), [responsive-ui](docs/systems/responsive-ui.md), [session-leak-fix](docs/systems/session-leak-fix.md)
  - ops: [telemetry](docs/ops/telemetry.md), [build-and-release](docs/ops/build-and-release.md), [tools](docs/ops/tools.md), [public-docs](docs/ops/public-docs.md)
  - research: [moddability](docs/research/moddability.md), [lap-2026-09-05-nordschleife](docs/research/lap-2026-09-05-nordschleife.md), [one-percent-low-hunt-2026-09-05](docs/research/one-percent-low-hunt-2026-09-05.md), [ui-lag-hunt-2026-09-06](docs/research/ui-lag-hunt-2026-09-06.md), [free-roam-unlock-2026-09-06](docs/research/free-roam-unlock-2026-09-06.md), [ghost-car-2026-09-06](docs/research/ghost-car-2026-09-06.md), [reflex-2026-09-12](docs/research/reflex-2026-09-12.md), [directstorage-1-3-2026-09-12](docs/research/directstorage-1-3-2026-09-12.md), [optimisation-deepdive-2026-09-12](docs/research/optimisation-deepdive-2026-09-12.md), [engine-flags-in-game-2026-09-12](docs/research/engine-flags-in-game-2026-09-12.md), [tile-pool-reshuffle-2026-09-12](docs/research/tile-pool-reshuffle-2026-09-12.md), [texture-streamer-flip-2026-09-13](docs/research/texture-streamer-flip-2026-09-13.md), [texture-streamer-overload-2026-09-13](docs/research/texture-streamer-overload-2026-09-13.md), [frame-time-mod-against-passive-2026-09-13](docs/research/frame-time-mod-against-passive-2026-09-13.md), [memory-creep-2026-09-14](docs/research/memory-creep-2026-09-14.md), [session-leak-census-2026-09-16](docs/research/session-leak-census-2026-09-16.md), [pso-cache-ab-2026-10-08](docs/research/pso-cache-ab-2026-10-08.md), [mesh-level-of-detail-2026-09-14](docs/research/mesh-level-of-detail-2026-09-14.md), [texture-streamer-camera-cuts-2026-09-14](docs/research/texture-streamer-camera-cuts-2026-09-14.md), [one-percent-lows-2026-09-14](docs/research/one-percent-lows-2026-09-14.md), [ui-lag-deepdive-2026-09-14](docs/research/ui-lag-deepdive-2026-09-14.md), [responsive-ui-rounds-2026-09-15](docs/research/responsive-ui-rounds-2026-09-15.md)

## The trackers

- [todos/INDEX.md](todos/INDEX.md), the work tracker: 14 open (TODO-009, TODO-010, TODO-013, TODO-014, TODO-016, TODO-017, TODO-019 to TODO-022, TODO-024, TODO-026, TODO-031, TODO-032), 13 done, 4 dropped
- [bugs/INDEX.md](bugs/INDEX.md), the defect tracker: 16 open, 18 fixed, 3 won't fix
- [decisions/INDEX.md](decisions/INDEX.md), the decision record: 21 standing (DEC-001 to DEC-003, DEC-005, DEC-007, DEC-008, DEC-011 to DEC-025), 4 superseded
- [reviews/INDEX.md](reviews/INDEX.md), code review ledgers: none open and 14 closed, the full review
  of main of 2026-09-20 split one branch per angle, 151 findings, 12 of them breaks, every angle merged
  into 0.4 by 2026-10-08

## Memory and handovers

- [memory/INDEX.md](memory/INDEX.md), project facts: 13 (9 project, 1 reference, 3 feedback)
- [handover/2026-09-05-lap-analysed.md](handover/2026-09-05-lap-analysed.md), state after the first telemetry lap
- [handover/2026-09-05-four-laps-done.md](handover/2026-09-05-four-laps-done.md), state after the fixed pools and latency cap landed
- [handover/2026-09-13-frame-time-mesh-budget.md](handover/2026-09-13-frame-time-mesh-budget.md), the frame time work after runs N, M and P1 to P3, paused to measure release 0.3.1, superseded by the mesh deep dive
- [handover/2026-09-20-review-of-main-angle-one-done.md](handover/2026-09-20-review-of-main-angle-one-done.md), the full review of main with one of thirteen angles closed and merged into 0.4, what the batch loop caught, and the next angle to branch
- [handover/2026-09-29-review-of-main-ui-fixes-batch-1-paused.md](handover/2026-09-29-review-of-main-ui-fixes-batch-1-paused.md), the full review of main paused inside the UI fixes angle's first batch, eight of thirteen angles merged into 0.4, and the five open findings to take up first
