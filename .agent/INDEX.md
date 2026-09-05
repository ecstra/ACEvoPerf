---
title: ACEvoPerf agent index
updated: 2026-09-05
---

# Index

Read this first. It lists every durable file in `.agent/` and what it is for.

## Specs

The rules each kind of file must follow. A file that breaks its spec is a bug.

- [spec/doc.md](spec/doc.md): knowledge documents under `docs/`
- [spec/bug.md](spec/bug.md): defect reports under `bugs/`
- [spec/todo.md](spec/todo.md): work items under `todos/`
- [spec/decision.md](spec/decision.md): decision records under `decisions/`
- [spec/memory.md](spec/memory.md): durable project facts under `memory/`
- [spec/handover.md](spec/handover.md): session handover notes under `handover/`

## Docs

- [docs/moddability.md](docs/moddability.md): how Assetto Corsa EVO 0.9.0 is built and what can be changed
- [docs/architecture.md](docs/architecture.md): what the proxy DLL does, in load order, and why each piece exists
- [docs/telemetry.md](docs/telemetry.md): the log and CSV files the mod writes and what each column means
- [docs/tools.md](docs/tools.md): the Python tools for the package and the settings files

## Trackers

- [bugs/](bugs/): one file per defect, status in the frontmatter
- [todos/](todos/): one file per work item, with a done when line

## Decisions

- [decisions/2026-09-05-dstorage-proxy-as-loader.md](decisions/2026-09-05-dstorage-proxy-as-loader.md)
- [decisions/2026-09-05-flags-by-memory-write.md](decisions/2026-09-05-flags-by-memory-write.md)
- [decisions/2026-09-05-staging-buffer-128mb.md](decisions/2026-09-05-staging-buffer-128mb.md)
- [decisions/2026-09-05-canonical-pools-off-by-default.md](decisions/2026-09-05-canonical-pools-off-by-default.md)

## Memory

- [memory/game-build.md](memory/game-build.md): facts about the game build the mod targets

## Handover

- [handover/2026-09-05-telemetry-lap.md](handover/2026-09-05-telemetry-lap.md): state at the end of the first session

## House rules

- [house-rules.md](house-rules.md): review protocol pointer
