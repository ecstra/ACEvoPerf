---
title: A low resolution texture is shown for about a second before the sharp one streams in
status: investigating
severity: major
reported: 2026-09-05
updated: 2026-09-05
source: user, "it still loads an ugly texture before streaming the good one (for a second)"
---

## Symptom

When new track sections come into view, surfaces first appear with a low mip and the full detail
arrives roughly a second later. This is with the mod installed (staging buffer capped, texture
tile pool around 1100 MB instead of 400 MB), so pool size alone did not remove it.

## Evidence

- Textures are tiled resources fed by DirectStorage on the queue named `GpuUpload File Queue`,
  destination `TILES`, 64 KB tiles, uncompressed. Menu scene measurement (0.9.0, session 2026-09-05):
  peaks around 26 MB per second on a PCIe 3 NVMe drive, far below what the drive can deliver.
- Tile requests arrive in small batches per submit (the `tile_batches` and `tile_maxbatch` columns
  of `acevo_perf_timeline.csv` will quantify this on the lap).
- The engine picks mips from a feedback pass (`main_streamer_feedback_depth` string in the exe),
  so detection latency depends on frame rate and on how far ahead the pass looks.

## Suspects

- Engine side request budget per frame (a fixed number of tiles requested per submit). Confirmed if
  `tile_maxbatch` is constant and small while `tile_req` per second stays low during pop in.
- Feedback latency, the mip is only requested once the surface is on screen. Confirmed if the
  request burst starts at the moment the surface becomes visible rather than before.
- Priority of the tile queue is `NORMAL`. Raising it to `HIGH` or `REALTIME` in the proxy when
  the queue is created is a cheap experiment once the lap data is in.
- The `texture_tier0` engine flag ("Force Texture Tier 0") may change which mips are considered
  resident by default. Untested.

## Fix

Not yet.
