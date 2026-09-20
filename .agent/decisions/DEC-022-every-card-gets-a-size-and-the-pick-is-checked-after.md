---
name: DEC-022-every-card-gets-a-size-and-the-pick-is-checked-after
kind: decision
description: the auto sizes cover every card down to 256 MB of tiles instead of standing down on a small one, and the adapter they were picked from is checked against the one the game renders on once that is knowable, superseding DEC-009
updated: 2026-09-20
links: [DEC-009-pool-and-staging-sizes-by-card, DEC-005-fixed-pool-sizes-by-default, DEC-003-staging-buffer-128mb, directstorage-streaming, engine-flags, proxy-architecture, review-2026-09-sweep-review-render]
date: 2026-09-20
area: render
status: standing
superseded-by:
---

## Decision

Every adapter gets a figure. `AutoTilePoolMb` runs 256, 512, 1024, 1536, 2048 and 3072 MB for
cards under 3, 5, 7, 11 and 15 GB of dedicated memory and above, and `AutoStagingMb` runs 128,
192 and 256 MB on the 7 and 11 GB steps. Nothing stands down, whatever the card reports.

The adapter is still the one with the most dedicated memory, read off the first DXGI factory
that reaches the mod. Once the game's D3D12 device exists, `CheckAutoSizeAdapter` compares that
adapter's LUID against the one the game actually renders on and logs a warning naming both cards
when they differ. When no factory of the game's has arrived by the first `DStorageGetFactory`
call, `ResolveAutoSizesFallback` creates one of its own and reads the card off that.

## Why

Three findings of the render review of 2026-09-20 (F-01, F-02, F-03) and one the hunter added
on top of the first attempt at F-01.

The first attempt put a floor under the brackets and wrote nothing below it, on the reasoning
that an integrated GPU's dedicated memory is a carve out and not a budget. That was worse than
the bug. `force_canonical_pool_sizes` is written at the early flag pass, about three seconds
before the card is known, and with it on and `tile_pool_mb` unwritten the engine takes the whole
`texturePoolSize` define, 1433 MB at Low and 6144 at Ultra, while the game's own 1024 MB staging
request goes through uncapped. Writing nothing is not a neutral act here, so the brackets have to
reach all the way down. 256 MB is where the engine's own dynamic formula bottoms out.

DEC-009 said the late flag pass "comes after the pool exists", which is why the sizes could only
be read at the game's own factory. The session of 2026-09-18 measures the opposite: the late pass
ran at 21.804 and the engine sized its tile pool at 22.165, 361 ms later. That is what makes the
fallback possible at all.

The LUID cannot drive the choice. The same session has the tile pool sized 7 ms before the swap
chain is created, and the swap chain is the first place the render adapter's LUID can be read, so
by the time the right answer is knowable the pool is already made.

## Alternatives

- A floor with nothing written below it: rejected, it hands the machine the canonical define and
  an uncapped staging buffer, which is the failure the auto sizes exist to prevent.
- Picking the adapter by the render device's LUID: impossible in the window that matters, see
  above. Checking it afterwards and naming both cards is what is left.
- Picking the adapter Windows would hand the process by default, through
  `EnumAdapterByGpuPreference` or the first `EnumAdapters1`: rejected. The engine's own log names
  one adapter it is "Using" and on the reference laptop it lists the discrete card only, while
  the window's monitor belongs to the integrated one, so the game does not take the default and
  matching it would be a guess in the other direction.
- Detecting an integrated GPU rather than sizing from its memory: no signal for it without a
  D3D12 device, which does not exist yet. An AMD APU with a large UMA carve out is still read as
  a card of that size, which the brackets now handle proportionately.

## Consequences

- A card of any size gets sizes that scale, and the smallest machines get the smallest pool the
  engine would ever pick for itself rather than the largest it can.
- The two numbers below 5 GB were never measured on such a card. They are the 6 GB card's budget
  worked backwards, and they err small on purpose.
- A hybrid machine where the game does not take the adapter with the most memory gets a log line
  naming both cards and the two ini keys to set by hand, not a correction.
- The auto tile pool is written twice now, at the auto phase and again at the late pass, like
  every other flag.
