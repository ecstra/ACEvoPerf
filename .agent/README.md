---
name: agent-readme
kind: doc
description: what the .agent directory is and how a cold agent boots from it
updated: 2026-08-12
links: [agent-index, conventions, house-rules-agent]
---

# The Agent Directory

This directory is the project's brain: everything an agent needs to work
on this project, committed to the repo so any agent on any machine
resumes cold. Humans read docs/ at the repo root when the project keeps
one, agents read this.

## Booting

1. Read [INDEX.md](INDEX.md), the spine. One line per file in this
   directory, grouped. It tells you what exists and where.
2. Read [house-rules.md](house-rules.md) before any branch, commit, or
   review work.
3. Obey [spec/](spec/conventions.md). Every folder here has a spec file
   that states its format and lifecycle. A file that breaks its spec is a
   bug.

## The map

- `spec/`: the law of this directory, one spec per folder plus the shared
  conventions.
- `docs/`: the knowledge, categorized. What the product is and how it
  works.
- `memory/`: project facts learned along the way, one per file.
- `bugs/`: defects, one per file.
- `todos/`: work to do, one per file. The project's tracker.
- `decisions/`: decisions taken and why, one per file.
- `reviews/`: one folder per code review, the ledger inside.
- `handover/`: session handoffs, born with its first file.

## Two absolutes

- Nothing secret ever enters this directory. It is committed.
- Nothing here references any machine's local state. Machines point at
  this directory, never the reverse.
