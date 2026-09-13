---
name: BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles
kind: bug
description: in a race with a field of cars the texture pool is full and textures stayed blurry until they got their tiles, because the streamer ranked every texture by its least important request and loaded a texture's detail only in one piece, both fixed and seen fixed in game, with AI cars and some props still blurry in a full field accepted by the owner
updated: 2026-09-13
links: [texture-streamer-overload-2026-09-13, texture-streamer-flip-2026-09-13, DEC-017-streamer-reload-fix-refuses-the-drop, DEC-018-a-full-texture-pool-keeps-the-players-car-first, BUG-016-vram-overhead-grows-across-scene-loads, BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash, directstorage-streaming]
status: fixed
severity: bug
area: streaming
reported: 2026-09-13
parent:
---

## Problem

Owner wording, 2026-09-13: "The streaming was broken sometimes (back to blurry textures for a second
or two) when all 10 AI cars were close to one another." And after two thirty car races: "its not
'blur when many cars' its literally 'overloaded streaming, blurs till it can get its tile' or
something (the comment 'played for 40 mins with 30 cars and then it crashed' is likely a victim of
this bug as well)."

In the census race of 29 AI at the Nürburgring GP the player's car stayed blurry for most of the
first lap after the start and blurred again as the field passed, and the ground under the grass
stayed blurry all race, which does not happen in a session alone.

## Evidence

The full record is
[texture-streamer-overload-2026-09-13](../docs/research/texture-streamer-overload-2026-09-13.md),
from `logs/census-ai30-20260913` and `logs/fix1-ai30-20260913`, with the earlier traces in
`logs/dyntrack-20260913`, `logs/ai30-A-fix-on` and `logs/ai30-B-fix-off`.

- The pool sat at 15,360 of 16,384 tiles, capacity minus the load gate's margin, in every race, with
  VRAM at its budget, so the pool cannot grow on this card.
- A texture drawn by several objects kept the lowest of their requests, all 7,036 records with
  differing requests in the census race. The player's livery ranked 11250 instead of 60000 whenever
  an AI car of the same model was within LOD 1, and dropped out of the pool.
- A load starts only when every missing level fits the free space at once. The livery waited 72
  kicks at 256 by 256 after the start, with room for most of its detail on most of them.
- The reload fix made no difference to the overload. The pair of 2026-09-13 with it off looked the
  same to the owner and carried the same pressure.

The first reading of this bug, that cars near the camera outrank the track through the vehicle
priority table, was wrong. No material in these races carries the vehicle category, the player's
car ranks on a table of its own at 60000 and AI cars rank on the default table like props.

The 40 minute multiplayer crash stays unattributed. No logs arrived from that report, and a crash
after a long session on that 8 GB card could equally be BUG-016's overhead or VRAM over the budget.

## Fix

Branch `fix/streamer-overload-blur`, commit `9f7e83e`, 2026-09-13, both in
`src/engine/streamer.cpp` and on by default in `[engine]`.

- `streamer_rank_fix` replaces the sort in front of the kick's record dedupe with one that puts a
  texture level's most important request first, so the dedupe keeps that one.
- `streamer_partial_loads` turns a load that does not fit into a load of the levels that do.

The census that found both, `[developer] streamer_census`, came in with commit `a2bfc5e` and went
out again with commit `removed: the streamer census, BUG-020 fixed`.

## Verification

The second race of 29 AI on 2026-09-13. The owner saw the player's car sharp in 1 to 2 s on the
grid and the road 5 to 10 s after, everything loaded at the start and the grass ground perfect at
every pause. The log has every record keeping its highest request, 923 loads cut to what fits
carrying 429 MB, drops down from 11,091 to 2,600, tile traffic over four laps down from 13.0 to 3.1
MB/s, and frame rate inside race to race noise.

AI cars of other models and some building sides still look blurry in a full field, because the
player's car and driver hold 487 MB of the pool at the top rank and AI cars rank like props. The
owner accepted that on 2026-09-13, "Leave it as is I guess. It was acceptable.", and why the other
ways of sharing the pool lost is
[DEC-018](../decisions/DEC-018-a-full-texture-pool-keeps-the-players-car-first.md).
