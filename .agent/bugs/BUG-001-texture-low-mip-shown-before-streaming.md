---
name: BUG-001-texture-low-mip-shown-before-streaming
kind: bug
description: surfaces appear with a low mip for about a second before the sharp tiles arrive
updated: 2026-09-06
links: [directstorage-streaming, lap-2026-09-05-nordschleife, BUG-006-distant-objects-pop-in, TODO-005-lap-two-experiments]
status: open
severity: bug
area: streaming
reported: 2026-09-05
parent:
---

## Problem

When a new track section comes into view, surfaces show a low mip and the full detail arrives
roughly a second later. Reported by the owner with the mod installed, so the larger tile pool
alone did not remove it. Owner wording: "it still loads an ugly texture before streaming the good
one (for a second)".

## Evidence

- Lap session 2026-09-05 (Nordschleife, 14 minutes of driving): 13,314 tile requests totalling
  8.3 GB, present in 802 of 960 seconds. Batches per submit have a median of 5 and a peak of
  119 per second. Peak delivery seen in the session is 761 MB in one second, so the drive and
  DirectStorage keep up with anything the engine asks for.
- The engine drives mip selection from a GPU texture feedback pass (`main_streamer_feedback_depth`
  target, `feedbackStagingBuffer0` and `feedbackStagingBuffer1` in the exe), which means a
  readback of at least two frames before a request is even issued.
- The tile queue is created at priority `NORMAL` (proxy log, `CreateQueue` line).

- Owner, 2026-09-06: "The road texture infront is lower and it becomes higher as soon as i
  appear that part of that road."
- Lap 19 frames CSV: the streamer's tile requests come in bursts of 9 ms on median, 46 ms at
  most, 14 to 70 tiles each, one burst every 1.7 s of driving. The requests are not throttled
  per frame and the drive keeps up (BUG-001 evidence above), so the delay the owner sees is not
  the streaming path. The sharper mip for a stretch of road is requested when the feedback
  pass sees that stretch at a distance where it needs it, and with DLSS rendering at 1443 by
  812 that distance is shorter than at native resolution.
- Lap 20: anisotropic filtering Custom, 16x, mip bias minus one, together with the LOD scales of
  BUG-006. Owner: still pops in. Tile traffic for the session 3.9 GB.

- Owner, later on 2026-09-06: the same thing does not happen in online gameplay at maximum
  settings on an RTX 5090, and the grass and trees change colour between unloaded and loaded
  (BUG-006). So the mip the streamer keeps resident at a distance depends on the card, which
  means the pool and the eviction, not the feedback pass alone.

## Fix

Absent. The request path is not the delay (bursts of 9 ms). Parked with BUG-006 to debug after
the optimisation pass: the tile pool at 1024 MB against what the scene wants resident, and
whether tiles still in view get evicted. Candidates that were tried are in TODO-005.

## Verification

Absent.
