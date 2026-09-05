---
title: Game crashes when changing car or track, and sometimes at random
status: open
severity: crash
reported: 2026-09-05
updated: 2026-09-05
source: user, "Game crashes everytime i change my car or change the map, sometimes crashes at random"
---

## Symptom

Changing the car or the track from the menus ends the process. Random crashes during play as well.
Not yet known whether the frequency changed with the mod installed.

## Evidence

- No dump files for `AssettoCorsaEVO.exe` in the Windows CrashDumps folder as of 2026-09-05.
- The game has its own crash handler (`CrashGuard.cpp`, engine flags `dumplevel` default 2 and
  `veh_crashdumps` default false). Where it writes dumps is not yet known.
- Windows Application event log was queried on 2026-09-05, result recorded in the handover note.

## Suspects

- Mod side: the queue proxy or factory proxy misbehaving when the game tears down and recreates
  DirectStorage objects during a scene change. Confirmed if `acevo_perf.log` ends right after
  `queue ... closed` lines, or if the crash stops with the mod uninstalled.
- Game side: a 0.9.0 bug in scene teardown, made more likely by VRAM pressure. Supported if the
  crash reproduces with the original `dstorage.dll` restored.
- Out of video memory during the switch, when the old scene and the new one are resident together.
  Confirmed if `vram_used_mb` is at the budget in the last timeline lines before the crash.

## Fix

Not yet.
