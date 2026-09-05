---
name: spec-memory
kind: doc
description: format and rules for .agent/memory, project facts learned along the way
updated: 2026-08-12
links: [conventions]
---

# Spec: memory/

Facts the project taught us that no doc owns: gotchas, settled patterns,
field lessons, pointers to external truth. One fact per file. If a fact
grows into real documentation, it graduates into docs/ and the memory
file is deleted.

## Format

Standard frontmatter, `kind: memory`, plus:

```
type: project | feedback | reference
```

- project: a fact about this codebase or its surroundings.
- feedback: a way of working the owner has corrected or confirmed, with
  the why.
- reference: a pointer to external truth (an upstream issue, a vendor
  doc, a dashboard).

The body is three beats: the fact, why it matters, how to apply it.

## Boundaries, stricter here than anywhere

- Project memory only. Facts about the owner as a person (their accounts,
  their preferences outside this project, anything personal) are not ours
  to manage and never enter the repo. A machine's local agent memory may
  hold those and may point INTO this folder. Nothing here points back.
- No secrets, no machine paths, no chat transcripts. A memory is a
  distilled fact, not a log.

## Upkeep

A session that learns something durable writes the memory before it ends.
A memory proven wrong is corrected or deleted, never left to mislead.
Duplicates merge. The index carries one line per fact.
