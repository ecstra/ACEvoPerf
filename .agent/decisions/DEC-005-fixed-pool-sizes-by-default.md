---
name: DEC-005-fixed-pool-sizes-by-default
kind: decision
description: ship force_canonical_pool_sizes with tile_pool_mb=1024 as the default instead of the engine's dynamic sizing
updated: 2026-09-05
links: [DEC-004-canonical-pools-off-by-default, BUG-010-texture-pool-shrinks-on-race-load-and-restart, engine-flags]
date: 2026-09-05
area: streaming
status: standing
superseded-by:
---

## Decision

Default `force_canonical_pool_sizes=true` and `tile_pool_mb=1024` in `dist/acevo_perf.ini`.
Measured in the menu on 2026-09-05: with both flags the engine creates the tile pool at
`tile_pool_mb` from the start (`[Tile Pool] sized to 900 MB (14400 tiles)` in the test) and never
resizes it, and the mesh budget is the canonical 1433 MB cap.

## Alternatives

- The engine's dynamic sizing (DEC-004): sizes the pools during the scene transition while the
  outgoing scene is still resident, which gave a race 633 MB and a restarted session 526 MB on a
  6 GB card with a gigabyte of VRAM left unused (BUG-010).
- Biasing the budget the engine reads through `IDXGIAdapter3::QueryVideoMemoryInfo` from the
  proxy: would also enlarge the pools but scales every other budget decision the engine makes
  with it, and needs another vtable hook.
- Canonical sizes without `tile_pool_mb`: 1433 MB tile pool, too much next to the 1433 MB mesh
  cap on a 6 GB card.

## Consequences

- The tile pool no longer shrinks on race load or restart. The size is a per card choice, the ini
  documents 1024 MB for 6 GB, 1536 for 8 GB, 2048 for 12 GB and up.
- The engine no longer adapts the pools when VRAM runs short. The timeline's `vram_used_mb`
  against `vram_budget_mb` is the check, verified on a full lap before this stands.
- DEC-004 is superseded.
