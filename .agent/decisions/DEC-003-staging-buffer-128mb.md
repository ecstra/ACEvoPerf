---
name: DEC-003-staging-buffer-128mb
kind: decision
description: the DirectStorage staging buffer is capped at 128 MB by default
updated: 2026-09-05
links: [directstorage-streaming, game-requests-1gb-staging-buffer, BUG-003-menu-icons-stop-rendering]
date: 2026-09-05
area: streaming
status: standing
superseded-by:
---

## Decision

Default `staging_buffer_mb=128` in `dist/acevo_perf.ini`, applied in `DStorageGetFactory` and in
`FactoryProxy::SetStagingBufferSize`. Measured on 0.9.0 with an RTX 3060 Laptop (6 GB): the
engine's "other objects" VRAM is 2343 MB at the game's 1024 MB, 807 MB at 256 MB and 551 MB at
128 MB, and both streaming pools grow from 400 MB to about 1115 MB.

## Alternatives

- 256 MB: pools 1015 MB, kept as the documented option for larger cards.
- The game's 1024 MB: pools stay at 400 MB on 6 GB cards.
- Below 128 MB: the game issues 32 MB requests and requests larger than the staging buffer fail,
  128 keeps four of the largest in flight.

## Consequences

- More textures resident, fewer evictions, the engine's dynamic sizing still applies.
- If a future build issues requests above 128 MB they would fail. The proxy logs failures on every
  queue so this would be visible.
