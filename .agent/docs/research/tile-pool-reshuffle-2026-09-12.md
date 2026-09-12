---
name: tile-pool-reshuffle-2026-09-12
kind: doc
description: parked and motionless the engine re-reads the same tile regions every 2.2 seconds, and the cause is its own tile pool relocating resident tiles rather than any redundancy the mod could remove, measured at about 4 percent of frame time and 22 MB/s
updated: 2026-09-12
links: [directstorage-streaming, content-package, BUG-001-texture-low-mip-shown-before-streaming, BUG-007-blurry-road-and-textures, BUG-010-texture-pool-shrinks-on-race-load-and-restart, BUG-016-vram-overhead-grows-across-scene-loads, TODO-017-tier-2-variable-rate-shading]
---

# The tile pool reshuffles its resident set every two seconds

Found by accident while measuring Tier 2 variable rate shading, which appeared to gain 3 percent
and turned out to be gaining it somewhere else entirely.

## What happens

Parked at the Nürburgring pit exit, car stationary, camera locked, nothing moving on screen, the
engine issues about 14 to 22 MB/s of texture tile reads, forever. Over a 91 second settled window:

- 828 tile requests, **1,290 MB**
- **28 distinct source offsets**
- 800 of the 828 requests, and **97 percent of the bytes**, are repeats of a copy already made
- 0 of those 28 offsets ever goes to a different destination

The same source bytes, to the same resource, the same coordinate, the same subresource, the same
tile count, **41 times in 91 seconds**. `details_alpha.texturemips` is one 8 MB read into 128
tiles, repeated 41 times.

## What it costs

Three parked runs, same spot, same view:

| config | tile pool | frame time | fps | tile traffic |
|---|---|---|---|---|
| shipped | 1024 MB | 11.427 ms | 87.5 | 1353 MB |
| overlay off | 1024 MB | 11.444 ms | 87.4 | 1470 MB |
| mod fully passive | 405 MB | 10.978 ms | 91.1 | 0 MB |

**About 4 percent of frame time**, plus the disk reads and the GPU upload bandwidth.

The passive run is not a configuration anyone should want: a 405 MB tile pool is BUG-007's mush,
and its 5586 MB of video memory against a 5226 MB budget is the over commit behind the BUG-003,
004 and 005 crashes. It is listed to show where the frame time goes, not as an option.

## Three readings that were wrong, and what corrected each

Worth keeping because each was reached from real data and each was still wrong.

1. **"The streamer is starved."** Killed by residency: 4443 MB resident with the churn suppressed
   against 4426 MB with it running, and memory queue totals within 1 percent. Nothing was missing.
2. **"It is an engine defect."** Premature, and stated before the control had been run. With the
   mod fully passive, no overlay, no staging cap, no forced pools, the churn vanished, which made
   it look like ours.
3. **"Our tile pool is too small, so it evicts."** Killed by the game's own log. The churn happens
   at a **1024 MB** pool and not at a **405 MB** one. Eviction pressure does the opposite. The
   working set doing the cycling is about 31 MB, which nothing evicts from a gigabyte.

## What it actually is

A probe on `ID3D12CommandQueue::UpdateTileMappings`, slot 8, logging every mapping change and the
heap offset each tile is mapped to.

Over a 100 second settled window, 887 mapping calls across 32 distinct targets:

| repeated mapping targets | 25 |
|---|---|
| always to the **same** pool offset | **0** |
| to a **different** offset | **25** |

And across the whole session: 68,051 tiles mapped, **21 unmapped**.

So the engine is not unmapping and refetching, and it is not failing to notice a tile is resident.
It is **relocating** the tile: the same logical tile is mapped 40 times in 100 seconds, to a
different offset in the 1024 MB heap every single time.

```
res=...DC15F26F0 sub=0 ntiles=64  mapped 40 x, offsets [960, 970, 1548, 2201, 2697, 4110] ...
```

Each new offset is memory that held something else, so the re-upload is **genuinely required**.
The re-read is a consequence of the reshuffle, not a redundancy sitting beside it.

## The dedupe was built, and it works, and it is not shipping

The first reading of the relocation was that it made a fix impossible. That was wrong, and the
owner pushed back on it: the destination that repeats is the **tiled resource coordinate**, the
same logical tile of the same texture. The pool offset is new each time because the engine asks
for the tile again and its allocator obliges. So the root is a residency tracking miss, and
keeping the tile where it already is removes the upload.

**Built on `fix/tile-streaming-churn`, measured over one lap, then dropped.**

| | result |
|---|---|
| uploads dropped | 1,542 of 6,650 |
| bytes dropped | **1,435 MB of 5,274, 27 percent** |
| visible corruption | none, one lap |
| frame time, dedupe live | 12.41 ms mean, 12.20 median, p99 19.10 |
| frame time, dedupe off | 12.17 ms mean, 11.87 median, p99 21.22 |

Slightly worse on the mean and median, better at p99, and both sessions were different lengths,
so the whole thing sits inside the thermal drift. That is what "under one percent" looks like on
this machine: unmeasurable. The dry run predicted it and the live run confirmed it.

**Two bugs found by hand before it ever ran**, both of which would have shown as wrong textures
rather than a crash, and both worth carrying into anything similar:

1. **Redirecting a tile to its old slot is certain to corrupt.** The engine then believes that
   slot is free and hands it to another tile, two tiles point at one slot, and the second upload
   overwrites the first. The mod has to own allocation outright or not touch it at all.
2. **Stale slot ownership.** When a tile moved from engine slot 100 to 200, slot 100 kept
   pointing at it, so the next tile to take slot 100 looked like an eviction of a tile that was
   still alive and freed its data from under it.

**And a cliff that decided it.** With the dedupe live the mod owns pool placement, and its free
list only refills when the engine signals an eviction, which it does less often than it consumes.
The session ended at **16,096 tiles held and 286 free of 16,384, 98 percent consumed**. On
exhaustion the code falls back to the engine's slot, which can already hold one of ours. One lap
did not reach it. A longer session or a track change very likely would.

**Deferred, not abandoned.** The owner's call on the day: not shipped now, and it comes back as
its own round because the saving is real disk, memory and GPU work even though it does not show
as frame rate here. The branch was deleted and
[TODO-019](../../todos/TODO-019-tile-upload-dedupe-done-properly.md) carries everything needed to
rebuild it without re-deriving any of it, including the two corruption bugs above. The round
starts with the eviction signal, because until the free list can be kept honest the rest is
premature.

## Why it is not worth chasing further from here

Under one percent of frame time while driving, on a card that is thermally pinned so saved GPU
work returns as clocks rather than frames. What it does remove is real: 1.4 GB per lap of disk
reads, staging traffic and upload bandwidth. That is why it is deferred rather than closed, and
why the frame rate is the wrong measure to judge it by on this machine.

It also stays a Kunos report on its own: a texture streamer that re-requests tiles it already
holds, and a pool allocator that relocates them on every request.

The one thing left open on our side is whether the reshuffle relates to
[BUG-016](../../bugs/BUG-016-vram-overhead-grows-across-scene-loads.md), where overhead climbs
across scene loads. An allocator that reshuffles constantly is the right shape for fragmentation,
and the pool running 92 to 98 percent full is a number that bug never had.

## Why variable rate shading appeared to help

Tier 2 VRS at 4x4 measured 3 to 4 percent faster with tile traffic at 0.1 MB/s against 19. That
was never shading, and the frame rate confirmed it: 85 fps against 85, 1 percent low 59 against
60, the GPU pegged at 98 percent and 86 °C both ways at a lower clock and lower power.

The engine drives its streamer from **sampler feedback**, a record of which texture tiles shaders
actually sampled. At 4x4 one invocation covers sixteen pixels, so that record carries a fraction
of its usual detail, the streamer asks for less, and the reshuffle has less to move. The gain is
the churn being suppressed by blinding the thing that drives it, and the pixelated road markings
in the same runs are the other half of the same trade.

It is not a fix and it should not be treated as one. See
[TODO-017](../../todos/TODO-017-tier-2-variable-rate-shading.md).
