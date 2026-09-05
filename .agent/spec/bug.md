---
title: Spec for bug reports
updated: 2026-09-05
---

# Bug spec

Applies to every file under `.agent/bugs/`. A bug has evidence of wrong behaviour. If it only has a goal, it is a todo.

File name: short kebab case slug of the symptom, for example `fps-drop-in-new-track-sections.md`.

Frontmatter:

- `title`: one line, the symptom
- `status`: `open`, `investigating`, `fixed`, `wontfix`
- `severity`: `crash`, `major`, `minor`
- `reported`: ISO date
- `updated`: ISO date of the last change to the file
- `source`: who reported it, in their words when available

Sections, in this order:

- `## Symptom`: what is seen, quoted from the reporter where possible
- `## Evidence`: log lines, measurements, file names, with the session they came from
- `## Suspects`: candidate causes, each with what would confirm or rule it out
- `## Fix`: what was changed and how it was verified, only once status is `fixed`
