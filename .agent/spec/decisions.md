---
name: spec-decisions
kind: doc
description: format for .agent/decisions, one decision per file
updated: 2026-08-12
links: [conventions]
---

# Spec: decisions/

Decisions actually taken and the reasoning behind them. One per file. Not
a status log, not a changelog: a real entry records a choice made, the
alternative it beat, or the gotcha that forced it.

## Format

Standard frontmatter, `kind: decision`, plus:

```
date: <YYYY-MM-DD, when the decision was taken>
area: <the project's own area vocabulary, kept consistent>
status: standing | superseded
superseded-by: <DEC name, when status says so>
```

Body sections for new decisions:

```
## Decision
What was decided, one paragraph.

## Alternatives
What it beat and why they lost.

## Consequences
What this commits us to, what it forecloses.
```

Entries imported from a legacy record may keep their original prose body
whole under a single `## Decision` heading: the reasoning was written
once, in the moment, and rewriting history loses more than it formats.

## Lifecycle

Decisions are never edited into new decisions. A reversal is a new file,
and the old one flips to `status: superseded` with the pointer. The truth
about the system lives in docs/, decisions only explain why it is shaped
that way.
