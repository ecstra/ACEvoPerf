---
title: Cap the DirectStorage staging buffer at 128 MB by default
date: 2026-09-05
status: accepted
---

## Context

The game calls `SetStagingBufferSize(1024 MB)`. The runtime keeps two staging buffers resident in
video memory, so about 2 GB of a 6 GB card is spent before the first texture loads, and the
engine sizes its texture tile pool and mesh pool from what is left (400 MB each on the test
machine).

## Decision

Default `staging_buffer_mb=128`. Measured on 0.9.0 with an RTX 3060 Laptop: "other objects"
VRAM 2343 MB at 1024, 807 MB at 256, 551 MB at 128, and both streaming pools grow from 400 MB to
1115 MB.

## Alternatives

- 256 MB: pools 1015 MB. Kept as the documented option for larger cards.
- Leave the game value: pools stay at 400 MB on 6 GB cards.
- Below 128 MB: the game issues 32 MB requests, and requests larger than the staging buffer fail.
  128 keeps four of the largest in flight.

## Consequences

- More textures resident, fewer evictions. The engine's dynamic sizing still applies.
- If a future build issues requests above 128 MB they would fail. The proxy logs failures on every
  queue so this would be visible.
