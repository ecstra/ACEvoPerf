---
name: TODO-022-frame-time-with-and-without-the-mod
kind: todo
description: the owner reads about 10 ms frame time without the mod against about 13 ms with it, and wants to know whether something is wrong, which needs a controlled comparison that separates the mod's deliberate texture pool from anything that costs frames for nothing
updated: 2026-09-14
links: [mesh-level-of-detail-2026-09-14, frame-time-mod-against-passive-2026-09-13, texture-streamer-flip-2026-09-13, DEC-005-fixed-pool-sizes-by-default, DEC-009-pool-and-staging-sizes-by-card, BUG-007-blurry-road-and-textures, TODO-023-name-what-the-game-keeps-across-identical-loads]
status: open
by: owner
area: render
born: 2026-09-13
done:
---

## What

Owner wording, 2026-09-13: "after that, we can work on the frametime (i think frametime of the game
without mod is ~10ms. were at 13. something is wrong in that?)"

## Why

Without the mod the engine sizes its texture pool to about 405 MB on this card and hands DirectStorage
a 1024 MB staging buffer, which is the blurry road of BUG-007. The mod forces a 1024 MB pool and a
128 MB staging buffer, so part of any gap can be the price of sharper textures rather than a fault.
The controlled pair of 2026-09-13 put parked frame time at 11.1 to 11.3 ms with the mod, so 13 ms is
more likely a driving or thermal reading, and the comparison has to hold both still.

## Where it stands, 2026-09-13

The first pair is run, [frame-time-mod-against-passive-2026-09-13](../docs/research/frame-time-mod-against-passive-2026-09-13.md).
The mod costs 3.8 percent parked and 8 percent on a lap, 10.34 against 10.66 ms median parked and
10.36 against 11.22 ms on the lap, with the GPU at the same clock. Without the mod the engine shrank
both pools to 366 MB and streamed no textures at all, so the budgets are the first suspect. It is not
the reload fix, which measured 1.4 percent faster on its own, so this continues on its own branch.

Run P1, the mod with the engine's own budgets, got back 2.72 of the 3.64 fps parked and 5.72 of 7.76
on the lap, and the owner saw everything mushy again. So three quarters of the cost is the detail the
budgets buy, and the rest of the mod is about 1 percent.

Run P2, textures back to 366 MB with the 1433 MB mesh budget held, landed next to M. The mesh budget
takes 2.10 of the budgets' 2.72 fps parked and 4.65 fps on the lap, the texture pool 0.62 fps
parked, and the owner saw P2 only a tenth of the way to the mod's picture.

Run P3, the mod with a 366 MB mesh budget through a developer build since removed, matched N in
the protocol with sharp textures, and the owner saw no difference anywhere. But 366 MB starves the
mesh streamer, nothing streams on the GP and the Red Bull Ring churns 1.6 to 2.2 GB of uploads per
lap, and a 29 AI race ran CPU bound at 56 fps against 67 earlier, not a pair. So the gap is mesh
detail the engine loads as designed, no budget size fixes it, and what is left to look at is whether
the mesh streamer loads detail the screen cannot show.

**Corrected 2026-09-14.** The 56 fps race is not evidence of harm from a small budget. It ran after
nine scene loads, with more commit and several times the texture traffic of the 1433 MB races, at the
same process CPU, so it is dropped. "No budget size fixes it" rests on the rule that a budget under
what a scene asks for removes detail the engine's level rule asked for.

Run R, release 0.3.1 on the same protocol, had the gap too, 4.33 fps parked and 8.52 on the lap
behind N, and 0.68 fps behind M parked with the parked churn the reload fix removed. So the gap has
been there since the fixed pools shipped, not something added after the release.

## Parked as the last deep dive

Owner wording, 2026-09-13: "We will deep-dive later on for this (keep this as the last deep-dive, we
have 2-3 more to fix". What is left is the deep dive into how the mesh streamer picks its levels,
after BUG-009, BUG-016 and BUG-020 have had theirs. The lever that set the mesh budget came out with
it, commit `a1840a0` has it.

## The deep dive, 2026-09-14

Run alongside the other deep dives rather than last, on the owner's word of 2026-09-13. The full
record is [mesh-level-of-detail-2026-09-14](../docs/research/mesh-level-of-detail-2026-09-14.md).

- **The mesh part is authored detail.** The engine picks a mesh's level by distance against the switch
  distances in the mesh file, and what is loaded can only make the drawn level coarser. The 1433 MB
  budget lets the authored LOD0 and LOD1 load, 80 to 87 percent of LOD0 triangles are under a pixel
  where LOD1 takes over, and that is how the content is built. Changing it is the Custom level of
  detail setting, a quality choice. The agents recommend closing the mesh part as not the mod's, the
  owner's call.
- **The flags do less than the docs said.** `tile_pool_mb` holds the tile pool on its own, and
  `force_canonical_pool_sizes` then only sets the mesh budget, whose 1433 MB is the Low
  `texturePoolSize` define. The engine's own formula with the mod's staging buffers would give about
  1.0 to 1.1 GB, but it runs again at every video settings apply, so it cannot replace the flag.
- **The rest of the mod is 0.38 to 0.97 fps parked**, inside the drift between launches. Only a toggle
  inside one launch can measure it, with the rule for what changes written here before it is built.

Next, with no build, `-log_info=meshStreamer` rides on a run already planned
([TODO-023](TODO-023-name-what-the-game-keeps-across-identical-loads.md)) to show whether anything is
trimmed at 1433 MB.

## Done when

The mod fully passive and the shipped defaults have been run as a controlled pair on the undervolted
card, same spot and same thermal start, and any gap is split into what each setting costs, with
anything that costs frames for no benefit fixed or recorded as not the mod's.
