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

## Done when

The mod fully passive and the shipped defaults have been run as a controlled pair on the undervolted
card, same spot and same thermal start, and any gap is split into what each setting costs, with
anything that costs frames for no benefit fixed or recorded as not the mod's.
