---
name: BUG-012-pit-lane-return-freezes-over-a-second
kind: bug
description: returning to the pit lane from the pause menu freezes the game for 1.3 to 1.5 seconds
updated: 2026-09-06
links: [telemetry, engine-flags, content-package, BUG-002-fps-drop-entering-new-track-sections]
status: wontfix
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

- 2026-09-06, read from the package and the exe: the same load happens at every session start,
  1.18 s between the game log's "Loading DynamicTrack preset" line and the next line in every
  session on disk (22:53:48.567 to 22:53:49.746 on lap 12, the same gap on all nineteen laps),
  and the lap 12 frames CSV shows it as the 1349 ms frame at t=25.8 s. So the pit lane return
  is a session restart paying the session start's price again, not a second path.
- What the price is: `24h.dynamictrackpresetcompressed` is 63 MB, a `DynamicTrackPresetCompressedData`
  protobuf whose tiles carry a zlib blob each (`78 9c` at the blob start, the exe links zlib
  1.3.1 and LZ4). Each blob unpacks into a `DynamicTrackPresetTileData` whose patches hold one
  `DynamicTrackPatchValues` message per value, so the cost is inflating 63 MB and then parsing
  a few hundred megabytes of one field messages, single threaded, on the render thread. The
  uncompressed `.dynamictrackpreset` variant other tracks ship has the same per value
  messages, so serving one through the overlay would save the inflate and not the parse.

## Fix

Won't fix, 2026-09-06: the freeze is the engine parsing its own preset format at every
session start and restart, the same on every card and only shorter on a faster CPU. The mod
cannot change the parse and a smaller preset would change the track's rubber state. The one
lever, `disable_dynamic_track=true`, removes the load together with the track evolution and
stays an owner's choice in the ini's optional flags.

## Verification

Absent.
