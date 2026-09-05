---
title: Spec for handover notes
updated: 2026-09-05
---

# Handover spec

Applies to every file under `.agent/handover/`. Write one when work is paused or passed on, so the next session resumes without reconstructing the previous one's head.

File name: `YYYY-MM-DD-short-slug.md`.

Frontmatter: `title`, `date`, `branch`.

Sections, in this order:

- `## State`: what is built, installed, verified, and what is not
- `## In flight`: what was being done when the session paused, with the exact next command or step
- `## Open questions`: things only the user can answer
- `## Pointers`: the bugs, todos and decisions that matter for the next step
