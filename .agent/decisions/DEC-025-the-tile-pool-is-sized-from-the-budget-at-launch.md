---
name: DEC-025-the-tile-pool-is-sized-from-the-budget-at-launch
kind: decision
description: the auto tile pool is sized at every launch from the video memory Windows grants the game and the display's pixel count, less a reserve measured on the 6 GB card, up to the game's own 6144 MB and never below the old table, marked experimental, rather than a table by dedicated memory or a pool that changes size during play
updated: 2026-10-08
links: [BUG-040-big-cards-get-a-texture-pool-far-below-what-the-game-would-use, DEC-022-every-card-gets-a-size-and-the-pick-is-checked-after, DEC-005-fixed-pool-sizes-by-default, directstorage-streaming, engine-flags]
date: 2026-10-08
area: streaming
status: standing
superseded-by:
---

## Decision

`tile_pool_mb=auto` takes the budget Windows grants the game on the render adapter at launch, read
with `QueryVideoMemoryInfo` off the game's own DXGI factory before its device exists, and the primary
display's mode. The pool is the budget less a reserve for everything else, 4200 MB at 1920 by 1080
plus 150 bytes for every pixel above that, rounded down to 64 MB, at most 6144 MB and never less than
the table by dedicated memory the mod used up to 0.3.2. When Windows cannot say, the budget is taken as
87 percent of dedicated memory, the reference card's share, 5226 of the 5994 MB it reports. A budget
above the dedicated memory is cut back to it, since a discrete card is never granted more than its own
and an integrated GPU is granted the PC's shared memory, which sized a 6 GB pool on a Radeon with
496 MB the same day (BUG-040, BUG-034). Taken on 2026-10-08 on the owner's word for all
cards, shipped in 0.4 marked experimental in the changelog.

The reserve comes from the one measured card, the 6 GB reference laptop at 1080p, where Windows granted
5226 MB and a 1024 MB pool left about 600 MB spare. The pixel share is an estimate of the screen
buffers. One check from the field fits it: a 10 GB card given 5 GB by hand ran clean, and the rule
gives it about 4.5 GB at 1080p and 4.2 GB at 1440p.

## Alternatives

The table by dedicated memory, DEC-022's. Its steps above 6 GB kept the 6 GB card's thin margin on every
bigger card, which left a 12 GB card a third of the game's own Ultra pool, with memory unused and loads
turned away for space (BUG-040). It stays as the floor, so no card gets less than before.

A pool that grows and shrinks during play, which the owner asked about. The engine builds its tile pool
once at start up and sizing it again means tearing it down and reloading every texture, a blur and a
stall at the moment a heavier section begins, triggered through the engine's own code, which would need
the exe read again. A pool is a cache, so one sized for the heaviest scene helps every scene, and
shrinking it on a light track frees nothing the game needs.

No cap above 6144 MB, which the owner asked about too. 6144 MB is the most the game itself ever
allocates, its pool at the Ultra texture pool size, and nothing has run it larger, so the engine could
have a limit no one has met, and a failure there would land on exactly the big cards this is for. The
owner agreed to keep it for now.

## Consequences

Every card above 6 GB gets more texture memory than before, and a card whose budget is lower at launch,
because other programs hold video memory, gets less than a card with the same memory free. The log line
names the budget, its source, the display and the size, so one log from a big card shows where it
landed. The `[streamer]` line's loads turned away for space is the measure of whether a big card at
6144 MB wants more, and the evidence for raising the cap. A player who sees trouble sets a fixed
`tile_pool_mb`, which the changelog says. The reserve is right only for the measured card, so a report
of video memory over budget on a big card at 4K is the first thing to read it against.
