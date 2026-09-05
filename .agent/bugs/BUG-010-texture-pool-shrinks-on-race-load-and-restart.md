---
name: BUG-010-texture-pool-shrinks-on-race-load-and-restart
kind: bug
description: the engine sizes the texture tile pool during scene transitions, so a race gets 633 MB and a restart 526 MB while a gigabyte of VRAM stays free
updated: 2026-09-05
links: [BUG-007-blurry-road-and-textures, directstorage-streaming, DEC-004-canonical-pools-off-by-default, engine-flags]
status: open
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

Candidate applied 2026-09-05 (DEC-005): `force_canonical_pool_sizes=true` with
`tile_pool_mb=1024`. Menu test with 900 MB: `[Tile Pool] sized to 900 MB (14400 tiles)` at
creation, no resize afterwards, mesh budget 1433 MB, menu VRAM 3051 MB.

## Verification

Pending a race and a session restart by the owner with the flags on: the pool line must stay at
1024 MB across both, the road must stay sharp after the restart, and `vram_used_mb` must stay
under the budget for the whole lap.
