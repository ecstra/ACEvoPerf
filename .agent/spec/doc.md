---
title: Spec for knowledge documents
updated: 2026-09-05
---

# Knowledge document spec

Applies to every file under `.agent/docs/`.

- Frontmatter with `title` and `updated` (ISO date of the last content change).
- One subject per file. A fact lives in exactly one document, other documents link to it.
- Written for a reader who has the repo but not this conversation. No references to any machine's local state.
- Style follows CLAUDE.md section 6.3: no em dashes, no semicolons, colons only as labels inside list items, hyphens only in established compound words and literal names.
- Measured numbers name the build and hardware they were taken on.
- Update the `updated` date and the line in `INDEX.md` in the same commit as the content change.
