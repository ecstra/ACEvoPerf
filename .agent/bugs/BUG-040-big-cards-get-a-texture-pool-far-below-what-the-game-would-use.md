---
name: BUG-040-big-cards-get-a-texture-pool-far-below-what-the-game-would-use
kind: bug
description: players on cards of 12 GB and more see their video memory use and GPU load drop with the mod and the game sometimes hang, cured by raising the mod's texture pool, because the auto sizes give such a card 2048 or 3072 MB of tiles against the 6144 MB the game's own Ultra pool would take, brackets extrapolated from the 6 GB card that keep its tight margin on every bigger one, open again since the budget sizing that fixed it in 0.4 went behind an experimental switch, off by default (DEC-027)
updated: 2026-10-09
links: [DEC-027-the-tile-pool-goes-back-to-the-table-and-the-budget-rule-is-experimental, DEC-025-the-tile-pool-is-sized-from-the-budget-at-launch, DEC-022-every-card-gets-a-size-and-the-pick-is-checked-after, DEC-005-fixed-pool-sizes-by-default, directstorage-streaming, BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles, BUG-032-the-game-freezes-at-a-thirty-ai-race-start, reported-working-configurations]
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
several players. The one figure on record is a 10 GB RTX 3080 given 5 GB by hand on the owner's
suggestion, with no report after it.

## Evidence

### What the mod gives a big card

`TableTilePoolMb` in `src/render/adapter.cpp` picks the tile pool by the card's dedicated memory, 2048 MB
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

`b8274e7`, 2026-10-08, on `fix/big-card-texture-pool` (DEC-025). The auto pool is sized at every launch
from the video memory Windows grants the game, read off its own DXGI factory before its device exists,
less a reserve of 4200 MB at 1080p that grows with the primary display's pixel count, up to the game's
own 6144 MB and never below the old table. A 12 GB card at 1440p gets about 5.9 GB where it had 2048 MB,
and a 16 GB card 6144 MB where it had 3072. Marked experimental in the changelog, with a fixed
`tile_pool_mb` as the way back.

### Off by default again, 2026-10-08

Shipped in 0.4 and taken back to an opt in the same day (DEC-027). Within hours a player under Proton went
from 90 to 100 fps on 0.3.2 to about 20 with a broken rear view mirror, and another said the game would no
longer start, neither with a card named. DXVK reports the whole card as the budget, so the rule overfills a
card under Proton, and no card above 6 GB had run it before it shipped. `tile_pool_mb=auto` gives the table
again and the rule runs only with `tile_pool_from_budget=1` under `[experimental]`, on
`fix/pool-sizing-back-to-the-table`. This bug stays open until the rule, or something like it, is shown
on cards above 6 GB.

### From the field, 2026-10-09

The Proton player, an RTX 3080 10 GB on one 1440p screen, confirmed the table build fixed both the frame rate
and the mirror, so it went up as the pre-release `v0.4.0-linux` and 0.4.0 stays the release for Windows. On
Windows a 5070 Ti with 16 GB reported 0.4.0 running better than 0.3.2, with more of the card in use, the first
card above 6 GB on record running the rule. The next version is meant to turn the rule back on by default and
skip it under Proton, which Wine marks with `wine_get_version` in ntdll.

## Verification

On the reference machine, `logs/big-card-pool-20261008`: the log read `5994 MB dedicated, 5226 MB
granted by Windows, display 1920x1080 -> tile pool 1024 MB`, so the budget is readable that early and the
6 GB card keeps its size, and the game logged `[Tile Pool] sized to 1024 MB (16384 tiles)` with a clean
quit.

The cards this is for are not verified here. A log from a 12 GB or bigger card, its auto sizes line and
its `[streamer]` loads turned away for space, is what settles it.

### An integrated GPU took a 6 GB pool, 2026-10-08

The same day the owner ran the game on the laptop's AMD integrated GPU with the discrete card switched
off, `logs/igpu-detour-20261008`. The log read `'AMD Radeon(TM) Graphics' has 496 MB dedicated, 15814
MB granted by Windows ... -> tile pool 6144 MB`. Windows grants an integrated GPU the PC's shared memory
as its own, so the rule above sized a 6 GB pool out of the memory the game itself runs in, which on a
16 GB machine or a handheld could starve the game. Fixed on `fix/integrated-gpu-pool` by cutting a
budget above the dedicated memory back to it, since a discrete card is never granted more than its own,
so an integrated GPU gets the old table's 256 MB again and a discrete card is untouched.

The same launch also drew the scene in a 1443 by 812 corner of a 1920 by 1080 screen with ghosted
edges, the game's own fault and not the mod's. DLSS stayed selected in the settings, NGX reported
`not available on this hardware/platform`, and the game rendered at DLSS's input size with its jitter
on and nothing to scale or resolve it, while its own settings still said DLSS was available. Setting
the upscaler to FSR or off is the way out on an integrated GPU.
