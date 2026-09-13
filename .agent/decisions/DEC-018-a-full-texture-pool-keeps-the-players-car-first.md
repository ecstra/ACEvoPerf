---
name: DEC-018-a-full-texture-pool-keeps-the-players-car-first
kind: decision
description: with both of BUG-020's fixes in, a full texture pool still holds the player's car and driver at the top rank and ranks AI cars like props, and the owner chose to leave that sharing as the engine has it rather than rank the car's finest levels lower or rank AI cars as cars, because each of those only moves the blur
updated: 2026-09-13
links: [BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles, texture-streamer-overload-2026-09-13, DEC-017-streamer-reload-fix-refuses-the-drop, DEC-009-pool-and-staging-sizes-by-card]
date: 2026-09-13
area: streaming
status: standing
---

## Decision

After BUG-020's two fixes the mod leaves the way a full texture pool is shared exactly as the engine
ranks it. The car the player drives and its driver keep every level of their textures at the top
rank, about 487 MB of the 1 GB pool in a thirty car race, AI cars rank on the default table like
trackside props, and whatever does not fit waits. Owner wording, 2026-09-13: "Leave it as is I
guess. It was acceptable. Anything else means rehauling the engine".

## Alternatives

Each was ranked again offline over the 97 kicks of lap 5 of `logs/fix1-ai30-20260913`, while the
field passed.

- The player's car's 4096 levels ranked at 24000 or at 13000. That frees 127 to 152 MB, all of it
  taken by track textures and none by AI cars, and the player's car shows 2048 on its largest
  textures whenever the pool is full, which in a race is always.
- AI cars ranked on the engine's unused vehicle table, 40000 over 500 m. The AI BMWs get 204 MB and
  the track drops from 387 to 165 MB, so the road and grass blur again. Paired with the first, the
  track still loses 82 MB.
- Ranking by what the camera shows. Car materials write no GPU feedback and the demand carries no
  camera, so it would take new engine patches with new layouts to keep in step with every game
  build, and it still could not make the pool bigger.

## Consequences

In a full field AI cars of other models and some trackside buildings can look blurry, and the
changelog says so. Only a bigger pool changes that, and on a 6 GB card VRAM is already at its
budget. A future round that wants AI cars sharper starts from the replays in
[texture-streamer-overload-2026-09-13](../docs/research/texture-streamer-overload-2026-09-13.md).
