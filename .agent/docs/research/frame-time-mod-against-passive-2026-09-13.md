---
name: frame-time-mod-against-passive-2026-09-13
kind: doc
description: a controlled pair on the undervolted reference card puts the mod's frame time cost at 3.8 percent parked and 8 percent on a lap against the mod fully passive, and most of it is mesh detail the canonical 1433 MB mesh budget loads, which a 366 MB budget gets back unseen but by starving the mesh streamer, frozen on the GP and churning 2 GB a lap at the Red Bull Ring, so no budget size fixes it
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

## P3, the mod's textures with a 366 MB mesh budget

Session `logs/frametime-20260913/P3-meshes-366`. No flag reaches this, so a developer build added
`[developer] mesh_budget_mb`, a stub that returns the configured size where the engine's budget
function at rva `0x1C80CC0` returns the canonical one. M with `mesh_budget_mb=366`, the log confirms
the patch. The owner had the lever taken out again until the mesh deep dive, commit `a1840a0` holds
it and `b9ad772` removed it.

| | N, passive | P3, textures 1024 MB, meshes 366 MB | M, mod on |
|---|---|---|---|
| parked fps, median, p99 | 96.56, 10.34 ms, 13.51 ms | 96.21, 10.31 ms, 12.80 ms | 92.92, 10.66 ms, 13.41 ms |
| lap fps, median, p99 | 97.18, 10.36 ms, 13.68 ms | 95.88, 10.50 ms, 13.28 ms | 89.42, 11.22 ms, 14.27 ms |
| GPU clock, power, temperature parked | 1822 MHz, 97.9 W, 72.0 °C | 1822 MHz, 98.7 W, 71.1 °C | 1822 MHz, 98.9 W, 73.2 °C |
| tile traffic on the lap | 0 MB/s | 16.50 MB/s | 16.34 MB/s |

- **It gets the whole frame cost back in the protocol.** P3 is 3.30 fps faster than M parked, 3.02 to
  3.53, and 6.46 on the lap, 4.83 to 8.07. Against N it is 0.35 fps slower parked, 0.14 to 0.60, and
  1.30 on the lap, 2.73 slower to 0.20 faster.
- **The texture pool's own cost is inside the session drift.** P3 came out 0.58 fps faster than P1
  parked with the bigger texture pool, so the 0.62 fps P2 put on the texture pool is noise between
  sessions. The whole budget cost is the mesh budget.
- **The owner saw no difference** in the protocol, a full Nordschleife lap or three laps of the Red
  Bull Ring, possibly the ground under the grass a little blurrier. The textures stayed sharp.

## What 366 MB does beyond the protocol

The owner kept going in the same launch, a Nordschleife lap, a multiplayer event at the Red Bull Ring
and a 29 AI race at the Nürburgring GP.

- **What the extra budget holds is a small amount of mesh.** At the GP pit exit the 1433 MB budget
  held 130 MB more than 366 MB, from both pairs (M 4419 against P3 4290 MB, and P2 3764 against P1
  5426 MB less the 1792 MB of bigger staging buffers). So that scene wants about 500 MB of mesh, 366
  MB caps it, and those 130 MB of finer levels cost 2 to 3 fps parked.
- **At 366 MB the mesh streamer stops streaming on the GP.** Memory to GPU uploads on the lap were
  3.8 MB/s at 1433 MB in M and P2, and 0 in P1 and P3.
- **At the Red Bull Ring it churns**, per lap against the ten laps of `logs/streamer-boot1-1124` at
  1433 MB in the table below.

| Red Bull Ring, per lap | memory to GPU | peak | tiles |
|---|---|---|---|
| 1433 MB, single player, 9 laps | 139 to 156 MB | 18 to 26 MB/s | 2008 to 2270 MB |
| 366 MB, multiplayer event, 3 laps | 1634 to 2174 MB | 262 to 432 MB/s | 1770 to 2013 MB |

The bursts land at the same point of every lap, with nothing read from the package, so the same meshes
leave the GPU and come back each lap. The multiplayer event ran a different car with other cars on
track, so not every megabyte is the budget, but twelve to fifteen times the uploads in bursts that
repeat with the lap is the texture flip's pattern at a budget edge.

- **The race ran on the CPU's limit.** 56.3 fps with the GPU 63 to 74 percent busy and no mesh
  uploads, against 67 fps in the two 29 AI races at 1433 MB earlier in the day. Those ran at 15:00
  and this one at 8:00, so it is not a pair, and whether 366 MB costs anything in a race is open.

## R, release 0.3.1

Session `logs/frametime-20260913/R-release-0.3.1`, the owner asking whether the gap came in after the
release. The zip's own DLL and ini with only its two CSVs on, which is M without the reload fix and
without the big screens fix, same budgets, runtime, Reflex and priorities.

| | N, passive | R, release 0.3.1 | M, mod on |
|---|---|---|---|
| parked fps, median, p99 | 96.56, 10.34 ms, 13.51 ms | 92.23, 10.80 ms, 13.55 ms | 92.92, 10.66 ms, 13.41 ms |
| lap fps, median, p99 | 97.18, 10.36 ms, 13.68 ms | 88.66, 11.32 ms, 14.12 ms | 89.42, 11.22 ms, 14.27 ms |
| tile traffic parked, lap | 0, 0 MB/s | 14.70, 17.93 MB/s | 0.07, 16.34 MB/s |

- **0.3.1 had the gap.** 4.33 fps slower than N parked, 4.19 to 4.46, and 8.52 on the lap, 7.03 to
  10.05. The game log shows the same canonical 1433 MB mesh budget.
- **It is 0.68 fps slower than M parked**, 0.51 to 0.85, with the parked churn back at 14.70 MB/s,
  about what the reload fix measured on its own. On the lap the 0.76 fps is not told apart from
  nothing.

## Where it stands

Nothing in the gap is wasted work. It is the mesh detail the engine loads when given its canonical
budget, and on this scene that detail costs 3.5 percent parked and 7 percent on a lap without the owner
seeing it. The 366 MB the game picks without the mod is not a better value, it comes from the staging
buffer bug (BUG-007) and it starves the mesh streamer, frozen on the GP and churning at the Red Bull
Ring. A budget between the two saves nothing where a scene's mesh fits under it and caps the scenes
that need more, so no size is a fix. What would be is the engine loading mesh detail the screen cannot
show, the way the texture streamer did, which is a deep dive into the mesh streamer's level choice.
The last 1 percent, Reflex, the priorities and the bundled runtime, is still untested.
