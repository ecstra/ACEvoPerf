---
name: frame-time-mod-against-passive-2026-09-13
kind: doc
description: a controlled pair on the undervolted reference card puts the mod's frame time cost at 3.8 percent parked and 8 percent on a lap against the mod fully passive, with the engine's own shrunken texture and mesh budgets the leading suspects and the reload fix ruled out
updated: 2026-09-13
links: [TODO-022-frame-time-with-and-without-the-mod, texture-streamer-flip-2026-09-13, DEC-005-fixed-pool-sizes-by-default, DEC-009-pool-and-staging-sizes-by-card, DEC-014-reflex-ships-on-boost-ships-off, BUG-007-blurry-road-and-textures, directstorage-streaming]
---

# The mod's frame time against the mod doing nothing

TODO-022's first pair. The owner's reading was about 10 ms without the mod and 13 ms with it.
Sessions `logs/frametime-20260913/N-passive` and `logs/frametime-20260913/M-mod`.

## Setup

The GPU undervolted with fans set hard by the owner, `nvidia-smi` sampling every second, the card
cooled back to 48 and 49 °C before each launch. Same build, Nürburgring 24h Time Attack Practice at
7:30, the Ferrari 296 GT3, parked at the same pit exit spot for two minutes with the cockpit camera
still, then one clean lap. Windows are the same offsets from the HUD in both, 100 s parked and the
lap from leaving the pit lane to crossing the line.

- **N, passive.** The DLL loaded only to time frames. No flags, no staging cap, no pool sizes, the
  game's own DirectStorage 1.2.3, process and GPU priority unchanged, no Reflex, no overlay, no
  reload fix. The log confirms each.
- **M, mod on.** The shipped ini, with only the two CSVs added.

## Result

| | N, passive | M, mod on |
|---|---|---|
| parked fps, median, p99 | 96.56, 10.34 ms, 13.51 ms | 92.92, 10.66 ms, 13.41 ms |
| parked 1 percent low | 71.4 | 70.9 |
| lap fps, median, p99 | 97.18, 10.36 ms, 13.68 ms | 89.42, 11.22 ms, 14.27 ms |
| lap 1 percent low | 69.6 | 66.9 |
| GPU clock, power, temperature parked | 1822 MHz, 97.9 W, 72.0 °C | 1822 MHz, 98.9 W, 73.2 °C |
| texture pool, mesh budget | engine shrank them to 366 MB and 366 MB | 1024 MB and 1433 MB |
| VRAM used, budget 5226 MB | 5430 MB, over | 4419 MB |
| tile traffic parked, lap | 0, 0 MB/s | 0.07, 16.34 MB/s |

**The mod costs 3.63 fps parked, 3.8 percent**, 95 percent interval 3.45 to 3.82 over the per second
samples, **and 7.76 fps on the lap, 8 percent.** The GPU held the same clock with no throttle reason
in both, so the card is not the variable. The owner's 10 ms without the mod is right, 10.34 ms
median here.

## Reading

- **The engine streamed nothing without the mod.** With its own 1024 MB staging buffers the engine
  sized both pools from what VRAM remained, 366 MB each, and then issued no texture tile request in
  the whole stint, parked or driving. So N drew coarser textures and whatever mesh detail fit 366 MB.
  That is the look BUG-007 recorded as the road and tyres turning to mush, and it ran over the VRAM
  budget without costing frames here.
- **So the gap is mostly detail, and some of it is work.** M draws finer textures and finer meshes
  from 1024 and 1433 MB budgets, and on the lap it streams 16 MB/s to keep them. Parked, where M's
  streaming is near zero thanks to the reload fix, it is still 3.8 percent slower, which points at
  the budgets themselves before the streaming. The lap doubles it, which fits streaming and view
  changes adding to it.
- **Not the reload fix.** Its own controlled pair measured it 1.4 percent faster
  ([texture-streamer-flip-2026-09-13](texture-streamer-flip-2026-09-13.md)).
- **Also untested.** Reflex, which DEC-014 measured as no change on a thermally pinned card, the GPU
  and process priorities, the bundled runtime. None of them adds GPU work, but none has had its own
  pair on this card.

## What splits it

One setting at a time from M, each as a pair like this one. The mesh budget first
(`force_canonical_pool_sizes=false` with `tile_pool_mb=1024`), then the tile pool (`tile_pool_mb`
lower with the mesh budget held), then Reflex off, with the owner judging the picture as well as the
numbers, because a budget that buys frames by turning the road back to mush is not a fix.
