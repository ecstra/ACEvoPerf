---
title: Leave force_canonical_pool_sizes off by default
date: 2026-09-05
status: accepted
---

## Context

The engine flag `force_canonical_pool_sizes` ignores the dynamic VRAM budget and uses compiled in
pool sizes. Measured on the test machine it gives 1433 MB texture tile pool and 1433 MB mesh pool
versus about 1115 MB each from the dynamic path with the staging cap.

## Decision

Ship it commented out in the ini with the measured numbers, off by default.

## Alternatives

- On by default: 320 MB more texture residency, but the engine no longer shrinks the pools when a
  full grid or a night race pushes VRAM to the budget, and the result would be paging stutter.

## Consequences

- Users on 6 GB cards can opt in after checking `vram_used_mb` against `vram_budget_mb` in the
  timeline during their heaviest session.
