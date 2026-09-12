---
name: agent-index
kind: doc
description: the spine of the agent directory, what exists and where
updated: 2026-09-12
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

- [docs/INDEX.md](docs/INDEX.md), the knowledge library: 20 docs in four categories
  - foundation: [proxy-architecture](docs/foundation/proxy-architecture.md), the DLL in load order
  - systems: [directstorage-streaming](docs/systems/directstorage-streaming.md), [engine-flags](docs/systems/engine-flags.md), [content-package](docs/systems/content-package.md), [package-override-layer](docs/systems/package-override-layer.md), [settings-files](docs/systems/settings-files.md)
  - ops: [telemetry](docs/ops/telemetry.md), [build-and-release](docs/ops/build-and-release.md), [tools](docs/ops/tools.md)
  - research: [moddability](docs/research/moddability.md), [lap-2026-09-05-nordschleife](docs/research/lap-2026-09-05-nordschleife.md), [one-percent-low-hunt-2026-09-05](docs/research/one-percent-low-hunt-2026-09-05.md), [ui-lag-hunt-2026-09-06](docs/research/ui-lag-hunt-2026-09-06.md), [free-roam-unlock-2026-09-06](docs/research/free-roam-unlock-2026-09-06.md), [ghost-car-2026-09-06](docs/research/ghost-car-2026-09-06.md), [reflex-2026-09-12](docs/research/reflex-2026-09-12.md), [directstorage-1-3-2026-09-12](docs/research/directstorage-1-3-2026-09-12.md), [optimisation-deepdive-2026-09-12](docs/research/optimisation-deepdive-2026-09-12.md), [engine-flags-in-game-2026-09-12](docs/research/engine-flags-in-game-2026-09-12.md)

## The trackers

- [todos/INDEX.md](todos/INDEX.md), the work tracker: 5 open (TODO-009, TODO-010, TODO-013, TODO-014, TODO-016), 7 done, 3 dropped
- [bugs/INDEX.md](bugs/INDEX.md), the defect tracker: 8 open, 6 fixed, 4 won't fix
- [decisions/INDEX.md](decisions/INDEX.md), the decision record: 14 standing (DEC-001 to DEC-003, DEC-005, DEC-007 to DEC-016), 2 superseded
- [reviews/INDEX.md](reviews/INDEX.md), code review ledgers: none yet

## Memory and handovers

- [memory/INDEX.md](memory/INDEX.md), project facts: 8 (6 project, 1 reference, 1 feedback)
- [handover/2026-09-05-lap-analysed.md](handover/2026-09-05-lap-analysed.md), state after the first telemetry lap
- [handover/2026-09-05-four-laps-done.md](handover/2026-09-05-four-laps-done.md), state after the fixed pools and latency cap landed
