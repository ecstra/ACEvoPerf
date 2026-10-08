---
name: TODO-031-reproduce-the-pso-cache-glitches-at-medium-textures
kind: todo
description: try to see BUG-039 on the reference machine with the cache on and warm and texture quality at Medium, the setting one player saw it at on every launch, because a fix that keeps the cache's few seconds per load can only be built against a glitch that shows here
updated: 2026-10-08
links: [BUG-039-the-pso-cache-draws-some-materials-wrong, DEC-024-the-pso-cache-stays-off, pso-cache-ab-2026-10-08]
status: open
by: owner
area: render
born: 2026-10-08
done:
---

## What

The cache is off by default since DEC-024, so nothing is waiting on this. It is the one way back to the
cache's gain, about 4 to 5 s per load once its file exists, if the glitches of BUG-039 can be fixed.

The owner has never seen them except once, and a fix cannot be told from a guess without a glitch to
look at. One player saw glass fences on every launch with a Ferrari 296 GT3 quick race at the
Nurburgring GP, texture quality at Medium and a cache file from an earlier run. The owner's three
launches of 2026-10-08 used the same car and track at the owner's usual settings, with nothing reported
wrong.

1. Set `enable_pso_cache=true` under `[flags]` and texture quality to Medium in the game.
2. Launch once, load that session and quit from the menu, so the cache file is written.
3. Launch again into the same session, drive a lap and look at the catch fences, the trees, the pit
   rails and the car's glass and paint, and save the logs.

If it shows, the next step is finding which side lets the wrong pipeline through. Windows' graphics
layer already checks a saved pipeline against what the game asks for, which is what the game's `stale
blob, recompiling` warnings are, so the wrong pipeline gets past that check. It is then either the
driver building wrong code from a matching saved pipeline, which the mod cannot touch, or the game's
own records kept beside each pipeline, which a stricter check of the mod's own at the graphics layer
might catch. Starting the cache empty at every launch is no way back, since the gain comes only from
an earlier run's file.

## Done when

The glitch is either seen on the reference machine and a fix tried on a branch of its own, or not seen
after these two launches, which leaves the cache off with nothing to build a fix against.
