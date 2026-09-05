---
name: DEC-004-canonical-pools-off-by-default
kind: decision
description: force_canonical_pool_sizes ships commented out
updated: 2026-09-05
links: [engine-flags, DEC-003-staging-buffer-128mb]
date: 2026-09-05
area: streaming
status: standing
superseded-by:
---

## Decision

The engine flag `force_canonical_pool_sizes` stays commented out in `dist/acevo_perf.ini` with the
measured numbers next to it: 1433 MB texture tile pool and 1433 MB mesh pool, against about
1115 MB each from the dynamic path with the staging cap.

## Alternatives

- On by default: 320 MB more texture residency, but the engine no longer shrinks the pools when a
  full grid or a night race pushes VRAM to the budget, and the result would be paging stutter.

## Consequences

- Users on 6 GB cards can opt in after checking `vram_used_mb` against `vram_budget_mb` in the
  timeline during their heaviest session. The lap of 2026-09-05 peaked at 4592 of 5226 MB with the
  dynamic pools, which leaves room for the experiment in TODO-005.
