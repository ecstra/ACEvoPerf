---
name: BUG-012-pit-lane-return-freezes-over-a-second
kind: bug
description: returning to the pit lane from the pause menu freezes the game for 1.3 to 1.5 seconds
updated: 2026-09-05
links: [telemetry, engine-flags, BUG-002-fps-drop-entering-new-track-sections]
status: open
severity: bug
area: stability
reported: 2026-09-05
parent:
---

## Problem

Choosing back to pits in the pause menu freezes the whole game for well over a second, three
times out of three in the 2026-09-05 20:24 session (frames of 1426, 1454 and 1318 ms in
`acevo_perf.log`, no streaming activity on the hitch lines).

## Evidence

- The game log in the same second, on the engine side: the car is teleported to its pit slot,
  the physics runs `forcePosition` and collision passes, and `Loading DynamicTrack preset:
  content\tracks\nurburgring/dynamic_track/24h.dynamictrackpresetcompressed` appears. Dynamic
  track presets are among the largest single assets in the package (60 MB) and this one is loaded
  and decompressed on the render thread during the teleport.
- The freeze is followed by two stalls of 150 to 260 ms while the pit lane screen comes up (the
  engine sends its page command twice, 1.7 s apart).
- Not a menu cost: the same freeze appears on the first pit lane entry of a session at a smaller
  scale (207 ms on 2026-09-05 19:51:54), the preset load dominates only on the return.

## Fix

Absent. Candidates: the engine flag `disable_dynamic_track` (skips the preset at the cost of
track evolution), or finding whether the preset can be kept resident between teleports. To be
measured with the flag on.

## Verification

Absent.
