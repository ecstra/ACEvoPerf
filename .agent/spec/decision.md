---
title: Spec for decision records
updated: 2026-09-05
---

# Decision record spec

Applies to every file under `.agent/decisions/`. A record captures a choice that was actually made and why. It is not a status log or a todo list. The code and the docs are the source of truth, the record only explains why they look the way they do.

File name: `YYYY-MM-DD-short-slug.md`.

Frontmatter:

- `title`: the decision in one line
- `date`: ISO date the decision was taken
- `status`: `accepted`, `superseded` (with a link to the newer record)

Sections, in this order:

- `## Context`: the situation that forced a choice
- `## Decision`: what was chosen
- `## Alternatives`: what it beat and why each lost
- `## Consequences`: what this makes easier or harder
