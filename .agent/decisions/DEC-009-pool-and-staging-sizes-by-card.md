---
name: DEC-009-pool-and-staging-sizes-by-card
kind: decision
description: tile_pool_mb and staging_buffer_mb default to auto, picked from the render adapter's dedicated memory at the game's first DXGI factory, instead of the 6 GB numbers
updated: 2026-09-06
links: [DEC-003-staging-buffer-128mb, DEC-005-fixed-pool-sizes-by-default, directstorage-streaming, proxy-architecture]
date: 2026-09-06
area: streaming
status: standing
superseded-by:
---

## Decision

The shipped ini says `tile_pool_mb=auto` and `staging_buffer_mb=auto`. When the game creates
its DXGI factory the proxy reads the dedicated memory of the adapter with the most of it and
picks the tile pool (1024, 1536, 2048 or 3072 MB for cards under 7, 11 and 15 GB and above)
and the staging buffer (128, 192 or 256 MB on the same steps), then writes the tile pool flag
and keeps the staging size for the DirectStorage factory. A number in the ini still wins.

## Why

DEC-003 and DEC-005 fixed 128 MB and 1024 MB, the right numbers for the reference 6 GB card,
and the readme told owners of bigger cards to edit the ini. The owner asked for the same zip
to be right everywhere. The memory has to be read before the renderer sizes its pools, which
happens about two seconds after attach and before the first DirectStorage call, so the only
safe moment is the game's own factory creation: DXGI cannot be created inside `DllMain`, and
the late flag pass comes after the pool exists.

## Alternatives

- Keep the fixed numbers and the readme advice: rejected, every other card gets the 6 GB pool.
- Read the memory in `DllMain` through DXGI: rejected, factory creation under the loader lock.
- Scale continuously from the memory: rejected, the steps keep the measured 600 MB margin of
  the 6 GB card and are easy to reason about in the log.

## Consequences

- The `auto sizes` log line names the adapter, its memory and the two picks, so a report from
  another machine says what it ran with.
- Any other flag set to `auto` is reported as having no rule and left alone.
