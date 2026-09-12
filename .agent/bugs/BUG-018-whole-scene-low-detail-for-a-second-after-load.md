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

## Measured, 2026-09-12

One session with `timeline=1`, `logs/bigscreen-builtin-1541`. The game reports
`Loading complete ... nurburgring.scene total 16.51 s` and `Curtain loading Off` at 15:37:43.839.
Per second across that moment:

| clock | tile req | tile MB | file to memory req | VRAM used / budget |
|---|---|---|---|---|
| 15:37:42 | 78 | 46.3 | 431 | 4574 / 5226 |
| 15:37:43 | 77 | 17.2 | 866 | 4657 / 5226 |
| **15:37:44 curtain lifts** | **522** | **600.8** | **14** | 4541 / 5226 |
| 15:37:45 | 102 | 65.4 | 0 | 4541 / 5226 |
| 15:37:46 | 69 | 53.8 | 6 | 4548 / 5226 |
| 15:37:47 | 40 | 44.2 | 0 | 4548 / 5226 |
| 15:37:48 | 12 | 10.4 | 0 | 4548 / 5226 |

So the shape is exact. The engine finishes the load, shows the scene, and only then pulls **600 MB
of texture tiles in the first visible second**, tapering to nothing over about four. The file to
memory queue, which carries the meshes and the rest of the load, falls to zero at the same instant,
so what the owner is looking at during those seconds is texture streaming and nothing else.

It is request driven and the requests cannot exist earlier. The engine chooses mips from a GPU
texture feedback pass (BUG-001 evidence), which needs the scene rendered before it can say what it
needs, so the burst cannot start until the curtain is off. Reading ahead in the proxy was built and
thrown away for an unrelated but final reason: a DirectStorage queue only accepts its declared
source type, so the mod cannot feed file source requests from memory (TODO-013).

VRAM is not the constraint either: 4541 MB of a 5226 MB budget at the curtain, 685 MB spare, and it
barely moves while the burst runs.

## It is intermittent

Owner, 2026-09-12, after the measurement: "the bug 018 only happens sometimes, not all times."

That fits the burst rather than complicating it. The 600 MB is what the scene needs minus what is
already resident, and the tile pool is not emptied between scenes, so a track entered when the pool
still holds much of its content has a far smaller burst and nothing visible to see. It also means
the size of the burst, not just its existence, is the thing to compare if this is ever revisited:
one load from a cold start against the same track re-entered from the menu.

So "sometimes" is expected behaviour for a residency driven ramp, and it is another reason not to
trade the staging cap for it, since the worst case is a first load and the common case is already
invisible.

## Where it stands

The one lever left that the mod owns is the staging buffer, capped at 128 MB by
`staging_buffer_mb`. 600 MB through a 128 MB staging buffer in one second is four fills, and a
larger buffer might shorten the burst. That is exactly the knob whose smallness fixes the crashes
and the missing icons on a 6 GB card (0.3.0), and the runtime keeps two of them, so raising it to
256 MB costs 512 MB of the 685 MB spare. Not worth trading the crash fix for a second of sharpening
without the owner saying so.

Worth noting against any expectation of a big win: 600 MB in one second is already close to the
761 MB/s best ever measured on this machine, so the path is not loafing.

## Done when

Either the settle time is traced to something the mod can move, and it moves, or it is named as
the engine showing the scene before its own streaming has finished and closed the way BUG-012 was,
with the numbers written down.
