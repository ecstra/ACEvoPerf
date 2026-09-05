---
name: BUG-004-crash-on-car-or-track-change
kind: bug
description: the game ended when changing car or track, fixed by the staging buffer cap
updated: 2026-09-05
links: [TODO-003-capture-crash-evidence, BUG-005-crash-on-startup, DEC-003-staging-buffer-128mb]
status: fixed
severity: breaks
area: stability
reported: 2026-09-05
parent:
---

## Problem

Owner wording: "Game crashes everytime i change my car or change the map, sometimes crashes at
random". Not yet known whether the frequency changed with the mod.

## Evidence

- No dump for the game exe in the Windows CrashDumps folder and no `Application Error` event in
  the Windows Application log in the 30 days before 2026-09-05. Five `Application Hang` events on
  2026-08-27, none since. So the failures may be hangs or handled exits rather than faults.
- The game has its own handler (`CrashGuard.cpp`, flags `dumplevel` default 2 and
  `veh_crashdumps` default false). Where it writes is unknown.
- All owner sessions on 2026-09-05 whose logs were read ended with the normal
  `Uninitializing COHTML library!` line.

## Fix

Root cause: video memory exhaustion during the scene switch, when the outgoing and incoming
scenes are resident together on top of the two 1024 MB DirectStorage staging buffers. Fixed by
the default `staging_buffer_mb=128` in `dist/acevo_perf.ini`, commit 0140743, 2026-09-05.

## Verification

Owner on 2026-09-05 with the mod at defaults: "I tried switching the car 4 times and its stable.
I tried diff car + diff map and it still worked." Previously it crashed on every change.

