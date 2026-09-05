---
name: BUG-010-texture-pool-shrinks-on-race-load-and-restart
kind: bug
description: the engine sized the texture tile pool during scene transitions (633 MB in a race, 526 MB after a restart), fixed by fixed pool sizes
updated: 2026-09-05
links: [BUG-007-blurry-road-and-textures, directstorage-streaming, DEC-005-fixed-pool-sizes-by-default, engine-flags]
status: fixed
severity: bug
area: streaming
reported: 2026-09-05
parent:
---

## Problem

Owner wording: "I closed and re-opened the lap (not game, just lap) and road texture is reduced
to be just a slop/mush (see image, thats supposed to be text and entire ground seems mushy)".
Road and tyre textures also stay soft in a fresh race (BUG-007).

## Evidence

Game log of the session of 2026-09-05 18:12 (mod at defaults, staging 128 MB):

- 18:12:40 menu scene: `remainder 2794 MB -> texture pool 1117 MB`
- 18:13:23 race load: `remainder 1583 MB -> texture pool 633 MB (was 1024)`, `sized to 633 MB
  (10128 tiles)`, mesh budget 633 MB
- 18:18:49 after `GameModeRequestRestartSession`: `remainder 1317 MB -> texture pool 526 MB
  (was 633)`, mesh budget 526 MB
- Steady state VRAM in the race: 4083 to 4208 MB of a 5226 MB budget, so about a gigabyte free
  after the pools were fixed at their small size.

The remainder is computed from the video memory in use at the moment of the transition, when the
outgoing scene (the menu showroom, or the previous session) is still resident. Both pools get 40
percent of that remainder (`DeviceAllocator.cpp`).

## Fix

Root cause: the engine's dynamic pool sizing runs during the scene transition. Fixed by the
defaults `force_canonical_pool_sizes=true` and `tile_pool_mb=1024` in `dist/acevo_perf.ini`
(DEC-005), commit d5197d5, 2026-09-05.

## Verification

Lap four of 2026-09-05 with the flags on: the game log has one `[Tile Pool] sized to 1024 MB
(16384 tiles)` at start and no pool line at the race load or after `RestartSession`, mesh
budget 1433 MB both times. VRAM 4556 to 4614 MB while driving, one second at 5222 MB during the
race load. Owner: "restarting session did not drop quality".
