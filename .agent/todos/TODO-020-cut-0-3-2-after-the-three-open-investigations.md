---
name: TODO-020-cut-0-3-2-after-the-three-open-investigations
kind: todo
description: 0.3.2 is not cut until the streaming layer, the 1 percent lows and the VRAM overhead have each had their round, because the release is meant to be final rather than a staging post
updated: 2026-09-12
links: [TODO-018-look-properly-at-the-streaming-layer, TODO-019-tile-upload-dedupe-done-properly, BUG-009-one-percent-lows-far-below-average, BUG-016-vram-overhead-grows-across-scene-loads, build-and-release]
status: open
by: owner
area: release
born: 2026-09-12
done:
---

## What

Owner wording, 2026-09-12: "dont ship yet. ship is final after 2,3,4."

The three, in his numbering from that message:

1. [TODO-018](TODO-018-look-properly-at-the-streaming-layer.md), look properly at the streaming
   layer
2. [BUG-009](../bugs/BUG-009-one-percent-lows-far-below-average.md), the 1 percent lows
3. [BUG-016](../bugs/BUG-016-vram-overhead-grows-across-scene-loads.md), the VRAM overhead that
   climbs across scene loads

Each gets its round. Then 0.3.2 is cut.

[TODO-019](TODO-019-tile-upload-dedupe-done-properly.md), the dedupe, is explicitly **not** a
gate. The owner's words: "thats later, just not now (it gets its own proper full round)." It may
land before or after this release depending on how its round goes.

## What is already sitting on main waiting for it

All verified on the reference machine, all unreleased:

- DirectStorage 1.3.0 as the bundled runtime, read back from the runtime that really loaded
- NVIDIA Reflex, in a game that ships none, confirmed by the driver
- High GPU scheduling priority
- The optional working set floor
- The trackside big screen fix, BUG-017
- `enable_pso_cache` off by default, BUG-015

Plus the session of 2026-09-12, which shipped no code and closed a lot: global illumination
measured and closed, `gibake_probes_per_frame` dead, `log_pso_on_creation` dead, the Agility SDK
built and removed, Tier 2 VRS built and removed, and the tile pool reshuffle found.

## Done when

The three above have each had a round and a written outcome, `release.ps1` has been run, and the
zip is on the Overtake listing.
