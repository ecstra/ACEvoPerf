---
name: BUG-001-texture-low-mip-shown-before-streaming
kind: bug
description: surfaces appear with a low mip for about a second before the sharp tiles arrive
updated: 2026-09-05
links: [directstorage-streaming, lap-2026-09-05-nordschleife, TODO-005-lap-two-experiments]
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

## Fix

Absent. Candidates are in TODO-005.

## Verification

Absent.
