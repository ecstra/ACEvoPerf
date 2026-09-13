---
name: TODO-022-frame-time-with-and-without-the-mod
kind: todo
description: the owner reads about 10 ms frame time without the mod against about 13 ms with it, and wants to know whether something is wrong, which needs a controlled comparison that separates the mod's deliberate texture pool from anything that costs frames for nothing
updated: 2026-09-13
links: [texture-streamer-flip-2026-09-13, DEC-005-fixed-pool-sizes-by-default, DEC-009-pool-and-staging-sizes-by-card, BUG-007-blurry-road-and-textures]
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

Run P3, the mod with a 366 MB mesh budget through the new `[developer] mesh_budget_mb`, matched N in
the protocol with sharp textures, and the owner saw no difference anywhere. But 366 MB starves the
mesh streamer, nothing streams on the GP and the Red Bull Ring churns 1.6 to 2.2 GB of uploads per
lap, and a 29 AI race ran CPU bound at 56 fps against 67 earlier, not a pair. So the gap is mesh
detail the engine loads as designed, no budget size fixes it, and what is left to look at is whether
the mesh streamer loads detail the screen cannot show.

Run R, release 0.3.1 on the same protocol, had the gap too, 4.33 fps parked and 8.52 on the lap
behind N, and 0.68 fps behind M parked with the parked churn the reload fix removed. So the gap has
been there since the fixed pools shipped, not something added after the release.

## Done when

The mod fully passive and the shipped defaults have been run as a controlled pair on the undervolted
card, same spot and same thermal start, and any gap is split into what each setting costs, with
anything that costs frames for no benefit fixed or recorded as not the mod's.
