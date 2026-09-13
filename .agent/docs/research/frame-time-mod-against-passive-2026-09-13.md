---
name: frame-time-mod-against-passive-2026-09-13
kind: doc
description: a controlled pair on the undervolted reference card puts the mod's frame time cost at 3.8 percent parked and 8 percent on a lap against the mod fully passive, three quarters of it is the bigger budgets, and splitting them shows the 1433 MB mesh budget costs 2 percent parked and 5 on a lap for a small part of the picture while the texture pool buys most of it for under 1, with the reload fix ruled out
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

## P1, the mod with the engine's own budgets

Session `logs/frametime-20260913/P1-mod-engine-budgets`. Everything of M except the budgets, which
went back to what N had: `staging_buffer_mb=0` and `force_canonical_pool_sizes=false` with no
`tile_pool_mb`. The game log confirms 366 MB for both pools, as in N. Both earlier runs were also
checked for the CPU holding the GPU back, and neither did, 98 to 99 percent GPU busy with the same
CPU load, so the cost is GPU work per frame. Windows as before, 100 s parked from 20 s after the HUD
and the lap from 6 s after leaving the pit box to the lap completing, all taken from the game log.

| | N, passive | P1, engine budgets | M, mod on |
|---|---|---|---|
| parked fps, median, p99 | 96.56, 10.34 ms, 13.51 ms | 95.63, 10.33 ms, 13.38 ms | 92.92, 10.66 ms, 13.41 ms |
| lap fps, median, p99 | 97.18, 10.36 ms, 13.68 ms | 95.14, 10.49 ms, 13.92 ms | 89.42, 11.22 ms, 14.27 ms |
| GPU clock, power, temperature parked | 1822 MHz, 97.9 W, 72.0 °C | 1822 MHz, 96.8 W, 70.4 °C | 1822 MHz, 98.9 W, 73.2 °C |
| texture pool, mesh budget | 366 MB, 366 MB | 366 MB, 366 MB | 1024 MB, 1433 MB |

- **The budgets are three quarters of the cost.** P1 is 2.72 fps faster than M parked, 95 percent
  interval 2.50 to 2.94, and 5.72 fps faster on the lap, 3.99 to 7.43. That is 2.72 of the 3.64 fps
  gap parked and 5.72 of 7.76 on the lap.
- **Those frames buy the detail.** The owner saw the road, the tyres and the cars as mushy as without
  the mod, with only the trackside big screens sharp, which is the overlay's own fix.
- **The rest of the mod costs about 1 percent parked**, 0.93 fps, 0.73 to 1.13, with the median frame
  the same 10.33 against 10.34 ms, so it sits in a few slower stretches rather than in every frame.
  P1 ran 50 minutes after N from a card at 42 °C instead of 48, so part of that can be the session.
  On the lap it is 2.04 fps, 0.52 to 3.61, over a lap driven 9 s slower.

## P2, textures back to 366 MB with the mesh budget held

Session `logs/frametime-20260913/P2-textures-366`. M with `tile_pool_mb=366`, so textures as in N and
meshes as in M. There is no flag for the mesh budget alone, the canonical path fixes it at 1433 MB and
`tile_pool_mb` sets only the tile pool. The game log confirms a 366 MB tile pool and the canonical
1433 MB mesh budget.

| | P1, both 366 MB | P2, textures 366 MB, meshes 1433 MB | M, textures 1024 MB, meshes 1433 MB |
|---|---|---|---|
| parked fps, median, p99 | 95.63, 10.33 ms, 13.38 ms | 93.53, 10.63 ms, 13.18 ms | 92.92, 10.66 ms, 13.41 ms |
| lap fps, median, p99 | 95.14, 10.49 ms, 13.92 ms | 90.49, 11.09 ms, 13.89 ms | 89.42, 11.22 ms, 14.27 ms |
| GPU clock, power, temperature parked | 1822 MHz, 96.8 W, 70.4 °C | 1822 MHz, 97.9 W, 72.5 °C | 1822 MHz, 98.9 W, 73.2 °C |
| VRAM used parked | 5426 MB | 3764 MB | 4419 MB |

- **The mesh budget is most of the cost.** P1 is 2.10 fps faster than P2 parked, 1.91 to 2.28, and
  4.65 fps on the lap, 2.94 to 6.35. So of the budgets' 2.72 fps parked the meshes take 2.10 and the
  texture pool 0.62, 0.46 to 0.79. On the lap the texture pool's 1.07 fps, 0.67 slower to 2.90
  faster, is not told apart from nothing.
- **The texture pool is most of the picture.** The owner saw P2 as only slightly better than P1,
  about a tenth of the way to the mod's usual picture, everything still mushy and blurry. That tenth
  is the finer meshes and whatever the streamer managed in a full 366 MB pool, where it turned away
  32,833 of the 35,000 finer levels it wanted.
- **So the texture pool buys most of the sharpness for under 1 percent**, and the 1433 MB mesh
  budget costs about 2 percent parked and 5 percent on the lap for a small part of it. That budget
  is not the fix for this card, it rides along with the canonical flag, which the mod sets to fix
  the tile pool (DEC-005). On this card the engine itself would pick 366 MB with its own staging
  buffers.

## What splits it

The test that decides it is the mod's textures with a small mesh budget, 1024 MB of tiles and the
366 MB of meshes the engine picks. No flag reaches that, so it needs the mesh budget written in the
engine the way the canonical path writes 1433 MB. The owner judges it against M, because a budget
that buys frames by turning geometry blocky or popping is not a fix either. The last 1 percent,
Reflex, the priorities and the bundled runtime, only if it still matters after that.
