---
name: BUG-040-big-cards-get-a-texture-pool-far-below-what-the-game-would-use
kind: bug
description: players on cards of 12 GB and more see their video memory use and GPU load drop with the mod and the game sometimes hang, cured by raising the mod's texture pool, because the auto sizes give such a card 2048 or 3072 MB of tiles against the 6144 MB the game's own Ultra pool would take, brackets extrapolated from the 6 GB card that keep its tight margin on every bigger one
updated: 2026-10-08
links: [DEC-022-every-card-gets-a-size-and-the-pick-is-checked-after, DEC-005-fixed-pool-sizes-by-default, directstorage-streaming, BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles, BUG-032-the-game-freezes-at-a-thirty-ai-race-start, reported-working-configurations]
area: streaming
status: open
severity: bug
reported: 2026-10-08
parent:
---

## Problem

Players with 12 GB cards and larger, the top of the range, see video memory use and GPU load drop with
the mod in. Raising the mod's video memory figure in `acevo_perf.ini` cures it. Some also have the game
hang and stop, and raising the same figure helps there too. Passed on by the owner on 2026-10-08 from
several players. Which setting they raised and to what is not recorded yet.

## Evidence

### What the mod gives a big card

`AutoTilePoolMb` in `src/render/adapter.cpp` picks the tile pool by the card's dedicated memory, 2048 MB
from 11 to 15 GB and 3072 MB from 15 GB up, and nothing above that. The game's own texture pool at Ultra
is the `texturePoolSize` define, 6144 MB, which is what the canonical flag hands over when nothing is
written (DEC-022). So a 12 GB card at Ultra gets a third of the texture memory the game would use, and a
24 GB card half.

The brackets come from one measurement on the 6 GB card, 1024 MB of tiles plus the 1433 MB mesh cap
leaving about 600 MB of budget, and each step up was written to keep that same margin on the next card
size. Kept literally, that margin is a few hundred MB on a 6 GB card and several GB unused on a 12 GB one,
which is the drop in memory use the players see.

### Why it would also hang

A pool smaller than the scene needs runs full and turns loads away for space, the state BUG-020 measured
on the 6 GB card in a race with AI. A GPU waiting on textures that cannot be admitted works less, which
fits the lower GPU load. The hang is a reading, not yet seen in a log from such a card. BUG-032, a freeze
at the start of a thirty AI race on the owner's 6 GB card with video memory over budget, is a different
shape, too much in use rather than too little.

### What cannot be checked here

The reference machine has 6 GB, and the desktop with an RTX 5070 that once showed a 1 percent low gap is
not available for runs. A fix can be checked here only by the sizes the mod logs for a given card, not by
play on one.

## Fix

Absent.

## Verification

Absent.
