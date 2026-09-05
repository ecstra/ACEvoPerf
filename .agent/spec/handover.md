---
name: spec-handover
kind: doc
description: format for .agent/handover, session handoffs
updated: 2026-08-12
links: [conventions]
---

# Spec: handover/

Notes written when work passes on or pauses mid task, so the next session
picks up without reconstructing a head. One file per handoff, named for
the work, standard frontmatter with `kind: doc`. The folder is born with
its first file.

A handover states: where the work stands, what is verified against what
is merely written, the next step, and the traps. It is a snapshot, dated,
and never updated after the fact: a resumed work stream that pauses again
writes a new one and links the old.

When the work completes, the handover's knowledge either graduates into
docs/ or dies with the file. Handovers are not documentation.
