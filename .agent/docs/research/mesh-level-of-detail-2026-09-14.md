---
name: mesh-level-of-detail-2026-09-14
kind: doc
description: TODO-022's deep dive with no new run, how the engine sizes the mesh budget, how the mesh streamer loads and how the renderer picks the level it draws, showing the 1433 MB budget's frame cost is the detail the meshes are authored with and not an engine fault, plus the launch option that makes the engine log its own mesh streamer and a plan for the untested last 1 percent
updated: 2026-09-14
links: [TODO-022-frame-time-with-and-without-the-mod, frame-time-mod-against-passive-2026-09-13, directstorage-streaming, engine-flags, BUG-006-distant-objects-pop-in, BUG-007-blurry-road-and-textures, BUG-010-texture-pool-shrinks-on-race-load-and-restart, DEC-005-fixed-pool-sizes-by-default, DEC-014-reflex-ships-on-boost-ships-off, TODO-023-name-what-the-game-keeps-across-identical-loads, release-build-ignores-gflags-cli]
---

# Mesh level of detail, the deep dive

TODO-022's round of 2026-09-14. Four angles from the exe, the mesh files in the package and the runs
of [frame-time-mod-against-passive-2026-09-13](frame-time-mod-against-passive-2026-09-13.md), each
checked by a second agent, then a critic and three gap rounds. No new session. RVAs are for the 0.9.1
exe and run names are the research doc's (N passive, M mod, P1 to P3 and R). The agents' scripts lived
in the session's scratch space and are not kept.

## How the mesh budget is set

- **The budget function.** At `0x1C80CC0`, with `force_canonical_pool_sizes` set (the byte at
  `0x67383D8`, read at `0x1C80CD7`) it returns the canonical size, otherwise
  `max(256, remainder x 0.8 x 0.5)` with no ceiling. N's game log reads
  `remainder 915 MB -> mesh budget 366 MB`.
- **1433 MB is a texture setting.** The canonical size is the `texturePoolSize` define reused,
  written at `0x1C72F0B` to `0x1C72F48` as 1433 MB for Low, 2048 for Medium, 3072 for High and 6144 for
  Ultra. 1433 MB is the owner's unset `texturePoolSize`, Low, not a size anyone chose for meshes.
- **`tile_pool_mb` does not need the canonical flag.** `0x1C80EC4` tests `tile_pool_mb` and returns
  before the canonical flag is read at `0x1C80ED4`, and pool creation at `0x1C72F58` takes
  `tile_pool_mb` whatever the flag says. So with the shipped ini `force_canonical_pool_sizes` only sets
  the mesh budget.
- **Sizing runs when video settings are applied.** Its only store is in `0x1CCF040`, called only from
  `0x1D43090`, the function that logs `Video Settings - Upscaling Enabled`. Every remainder line comes
  1 to 2 s after a `[rendering] Video Settings` block, once per launch on 0.9.1 (N, P1) and four times
  in the 0.9.0 session `lap3-pacing-20260905-1812` (1117, 633, 627 and 526 MB).
- **What the engine would pick with the mod's staging buffers.** About 1.02 to 1.12 GB at boot on this
  card, inferred from the formula and M's VRAM at its sizing second. The game without the mod gets
  366 MB only because its 1024 MB staging buffers take the room (BUG-007).

## How the mesh streamer loads

- **The loop.** `0x1F28000`, one caller at `0x1D823F9`, once a frame by inference. A mesh with no
  demand last frame drops to tier 0 at once (`0x1F288AF`). The wanted tier is the tier of the lowest
  bit in main view mask 0, or the finest when forced. Records sort forced first, then by registration
  priority (`0x1F16CA0`), each non forced record steps down while used plus its bytes exceed the budget
  (`0x1F283E0` to `0x1F28401`), then it loads what is above and frees what is below at once
  (`0x1EA2C90`). No hysteresis, no minimum time resident, nothing kept for meshes that left the view.
- **Tiers** (`0x1EA24E0`). A mesh with one level never streams. With two, LOD1 then LOD0 stream. With
  three or more, LOD2 and coarser load as one block that stays, and LOD1 and LOD0 stream on their own.
- **Demand.** The mask writer `0x1EE5460` puts view types 10 to 12 in mask 0, the only mask the
  streamer reads, shadows 6 to 9 in mask 1, cubemaps 4 and 5 in mask 2 and mirrors 2 and 3 in mask 3.
  A single placement asks for one level (`0x1EDCC30` to `0x1EDCC45`), an instance set for every level
  from its first to its last (`0x1EDE030` to `0x1EDE045`), so any instanced multi level mesh in view
  loads its LOD0 tier at any distance. That costs memory and uploads, not drawn triangles.
- **Its own log line.** The streamer holds an spdlog logger named `meshStreamer` created at warn
  (`0x1F1D726` to `0x1F1D760`), and an ungated call at `0x1F28AA5` writes `tracked N, used N MB,
  budget N MB` at info. The 0.9.1 command line whitelist keeps `-log_info=` and loggers created later
  read the name lists, so `-log_info=meshStreamer` in the launch options should print it every update.
  No log on disk has the line yet. `used` there is the committed plan, four allocator counters minus
  the tracked resident bytes, plus meshes in flight at their old size, plus tier 0 of unseen meshes,
  plus each admitted record.

## How the renderer picks the level it draws

- **What is loaded only coarsens.** The drawn level is the coarser of the distance level and the
  finest resident tier, a `cmovl` at `0x1EDCC58` for single placements and `0x1EDE058` for instance
  sets. A bigger budget never makes a finer level draw than the distance rule asks for.
- **Two distance rules.** Single placements measure to the world box centre minus its half diagonal,
  clamped at 0 (`0x1EDC8D5`), divided by the view scale, the car's scale for mesh type 4 and the
  track's otherwise. The GPU instance shader measures to the centre. World baked static meshes take the
  single placement path.
- **The view scale.** `0x1DBDF06` to `0x1DBDF4F` computes the LOD preset divided by
  `min(1, max(0, fov / 80))`, with no render resolution term, so DLSS switches levels at native
  distances. At the owner's settings the main view uses 1.25 over f for the track and 0.9 over f for
  the car, the shadow cascades 1.25 over f and 1.0, the mirror 0.05 over f.
- **Other views never draw finer than the main view.** Shadow views are built in `0x1D7A6A0` from the
  main camera's position and field of view, cubemaps use a field of view of 90, cascades above 0 skip
  the player's car, and the `carFixedLod` settings getters are unreachable on 0.9.1. 351 of the 406
  multi level Nürburgring meshes map LOD0 to a coarser shadow record, and none of the 406 has a
  dithered LOD transition.

## What the extra budget buys

- **Mostly detail finer than the screen shows.** Of 5,096,269 LOD0 triangles in the multi level
  Nürburgring meshes, counted one by one from decoded positions, 80.2, 82.5 and 87.1 percent are under
  one pixel at the far end of their range at a scale of 1.0, 1.25 and 2.0 (field of view 50, 812 rows,
  face on). LOD1 at its far end is 45.8 to 71.6 percent under a pixel.
- **Short ranges.** LOD1 takes over at a median of 80 m (5 m at p10, 250 m at p90), and 2,743 of the
  3,149 Nürburgring mesh files have one level only.
- **So it is authored content.** The budget lets the authored LOD0 and LOD1 load and the engine draws
  them where the files say. Moving the switch distances is the game's own Custom level of detail, a
  quality choice the mod does not make, the same precedent as the texture feedback shaders asking for
  finer levels than needed.
- **One engine side excess, small.** The half diagonal rule keeps LOD0 on merged chunks 200 to 410 m
  wide near the pit. Within 400 m of three pit lane spots, 50 to 74 percent of the multi level static
  LOD0 triangles, 59,902 to 115,853 triangles and 7.3 to 14.6 MB, are there only because of it, the
  largest `boulevard4_lod0` (27,250) and `congress_lod0` (26,194). The centre rule would draw the near
  parts of those chunks coarser than their own ranges ask, so a change there is partly a LOD bias.
- **BUG-006's halving explained, not re-checked.** Tree meshes in the main and shadow views draw one
  fixed level from their static LOD fields (`0x1EDDE44`), and all 324 vegetation meshes with that
  block read 0, 0, 0, so with the experimental static LOD every tree draws LOD0 at every distance at
  every tier.

## What the runs already say

- **The protocol windows reproduce.** Parked fps N 96.56, M 92.91, P1 95.61, P2 93.53, P3 96.20, R
  92.23. Lap N 97.18, M 89.42, P1 95.14, P2 90.49, P3 95.86, R 88.67, with the GPU at 1822 MHz and 98 to
  99 percent busy in every window.
- **The mesh budget stretches every frame.** Parked, M runs 2.1 to 4.8 percent above P3 from p05 to
  p99. On the lap M loses to P3 in 20 of 20 distance bins, 5.4, 6.8 and 8.2 percent by sector, and
  uploads add 0.006 to 0.033 ms. The cost is drawing, not streaming.
- **At 366 MB the Nürburgring streamer froze.** The buffer upload count stops at exactly 6,819 after
  the GP load in N, P1 and P3 and is still 6,819 at the end of the stint, against 7,722 and 7,838 at
  1433 MB. P3's Nordschleife stint uploaded 0 MB in 11 minutes.
- **At 366 MB the Red Bull Ring churn follows track position.** Both sessions were the same special
  event with the same `ks_mazda_rx7_fd`, car id and no AI. 366 MB laps upload 1,580 to 2,174 MB, all
  from RAM with VRAM flat, in bursts 23 to 28 s after the line and 53 to 55 s into the lap, against
  139 to 157 MB a lap at 1433 MB. The same family as the texture flip (sort, admit, drop at once, no
  memory of the last decision), but two sites a lap, and it only happens in the game without the mod.
- **The 56 fps race at 366 MB does not count.** It came after nine scene loads, at 17,459 MB of commit
  against 16,667 to 16,930, with 27.9 MB/s of tiles in its first 400 s against 2.7 to 20.0, straight
  after a 24h race loaded and quit, and at the same 40.8 percent process CPU as the 1433 MB races at 67
  to 72 fps.
- **The rest of the mod is 0.38 to 0.97 fps parked.** N minus P3 0.38 (0.13 to 0.64), N minus P1 0.97
  (0.73 to 1.20). Adjacent 20 s blocks with nothing changed differ with a standard deviation of 0.36
  fps, so separate launches cannot resolve it and seven pairs inside one launch resolve about 0.33 fps.

## Verdict

No engine fault explains the frame gap. The 1433 MB budget's 3.3 fps parked and 6.5 fps on a GP lap
are the authored LOD0 and LOD1 detail drawn by the engine's own distance rule, recorded as not the
mod's to change. No budget size is a fix, because a budget under what a scene asks for removes detail
the level rule asked for, and 366 MB freezes the Nürburgring streamer and churns the Red Bull Ring.

Worth telling Kunos. 80 to 87 percent of LOD0 triangles are under a pixel where LOD1 takes over, merged
chunks keep per building ranges, instance sets load every level, the vegetation's static LOD fields are
all 0, the `carFixedLod` settings are dead, and the canonical mesh budget reuses the texture pool
define.

Candidates left, none a fix for the gap.

- **The centre rule for single placements**, rewriting the `vsubss` at `0x1EDC8D5` to a move. 60,000
  to 116,000 fewer LOD0 triangles near the pit, well under 1 fps, partly a LOD bias. Worth at most an
  optional state in a toggle run.
- **Keeping streamed tiers of meshes that just left the view** while the budget has room. Unproven.
  Races upload 4.0 to 9.1 MB/s against 3.78 MB/s on M's lap, uploads showed no frame cost, and races
  already run at 5,144 to 5,345 MB of a 5,226 MB VRAM budget, so holding more could hurt.

## Runs

1. **The engine's own readout, no build.** `-log_info=meshStreamer` in the launch options of a run
   already planned ([TODO-023](../../todos/TODO-023-name-what-the-game-keeps-across-identical-loads.md)
   is the natural one), removed after. It says whether anything is trimmed at 1433 MB, the smallest
   budget that behaves the same, whether 29 AI races reach the edge, and whether the budget changes
   between tracks. No lines while the argument echo in the game log shows the switch kills the reading.
   `tools/telemetry_report.py` counts these lines as streaming events, and they add about 30 MB of log
   an hour.
2. **The last 1 percent, only if the owner wants the number.** One launch, parked, 20 s blocks of the
   shipped state A and the game's own runtime state X (Reflex sleep off, GPU and process priority
   normal, timer resolution released), in the order A X X A four times, optionally with a state C that
   adds the centre rule. The rule is written into TODO-022 before building. X beating A by more than
   0.3 fps sends Reflex and GPU priority to a split run, anything less closes it as noise.

## Still open

- Whether the player's car and the AI cars register with the mesh streamer and with the forced bit,
  which decides whether AI cars reupload as they leave the main view. From the producers at
  `0x1DC2D10`, `0x1DC34D0` and `0x1DC1EB0` up to the car renderer.
- The shadow cost of the merged chunks, whose LOD0 shadow records may be LOD0 themselves. The remap at
  `0x1EE6080` and the cascade loop in `0x1EDB9F0`.
- The parked protocol spot is the pit box, and the radius census used pit lane spots of the 24h layout.
- The cockpit's field of view, which sets every scale.
- The four allocator counters behind `used`, and why the Nürburgring froze while the Red Bull Ring
  churned at the same 366 MB. Run 1's `tracked 0` lines give the base.
- Why sizing ran four times a session on 0.9.0 and once a launch on 0.9.1.
- On track VRAM drops by exactly 32 or 64 MB with no commit change (BUG-016's sessions S and R), which
  read as whole mesh pool blocks leaving residency.
