---
title: Spec for todos
updated: 2026-09-05
---

# Todo spec

Applies to every file under `.agent/todos/`. A todo has a done when line. Multi step work gets filed here instead of being carried in a session's head.

File name: short kebab case slug of the outcome, for example `analyse-nordschleife-lap-telemetry.md`.

Frontmatter:

- `title`: one line, the outcome
- `status`: `open`, `in-progress`, `done`, `dropped`
- `created`: ISO date
- `updated`: ISO date of the last change to the file
- `source`: who asked for it, with their wording preserved when they wrote it down

Sections, in this order:

- `## Done when`: the verifiable end state
- `## Steps`: numbered, each with its own check
- `## Notes`: anything learned on the way that the next session needs
