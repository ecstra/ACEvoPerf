---
name: BUG-018-whole-scene-low-detail-for-a-second-after-load
kind: bug
description: for one or two seconds after the curtain lifts the whole scene is at low detail, meshes and textures together, at game start and on entering a track, distinct from the approach driven mip band of BUG-001
updated: 2026-09-12
links: [directstorage-streaming, telemetry, BUG-001-texture-low-mip-shown-before-streaming, TODO-013-faster-session-loads]
area: streaming
status: open
severity: medium
reported: 2026-09-12
parent:
---

## Symptom

Owner, 2026-09-12: "another bug is loading. The textures are blurry for a second or two on-load."
Then, correcting a wrong reading of it as BUG-001: "the on-load blur is not 'low mip until you
approach' rather the entire game is low poly on-load for 1-2 seconds (its not while driving,
happens when game opened for 1-2s and when i go to the track for 1-2s)."

So this is the whole scene at once, meshes as well as textures, for a bounded moment right after
the scene appears, and it settles on its own. It is not the band of mip that a surface carries at
a distance while driving, which is BUG-001 and was closed as the engine's behaviour on every card.

## Why it is worth its own file

BUG-001 was about a surface sharpening as you drive towards it, and the evidence there showed the
streaming path answering in 9 ms bursts with the drive keeping up. This is a different shape: a
whole scene starting coarse and resolving together, twice per session, at start-up and at track
entry.

The engine lifts the curtain before streaming has caught up. From the loading profiler of
2026-09-12 the Nürburgring reports `Loading complete ... total 15.69 s` and
`Curtain loading Off checkAndShowUIAndScene` on the same line, while the mod's own queue
statistics show the tile queue still running at 76 MB/s over the window that straddles the
handover. So the scene is shown and the detail arrives afterwards, which matches what the owner
describes.

What is not yet known is whether anything the mod owns changes that. Candidates already closed:
`tile_queue_priority` raised to realtime showed no measurable difference on lap two of 2026-09-05
(TODO-005), and `texture_tier0` was a regression.

## Where it stands

Measurement armed and not yet taken: `[log] timeline=1` gives one line per second with tile,
file to memory and memory to GPU request counts alongside VRAM used against budget. One session,
start the game, enter a track, quit, then read how long tile traffic takes to settle after each
curtain lift and whether the pool is full or still filling while it is coarse.

## Done when

Either the settle time is traced to something the mod can move, and it moves, or it is named as
the engine showing the scene before its own streaming has finished and closed the way BUG-012 was,
with the numbers written down.
