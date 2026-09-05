---
title: Capture evidence for the crashes so they can be attributed to the game or the mod
status: open
created: 2026-09-05
updated: 2026-09-05
source: user, "General Stability: Game crashes everytime i change my car or change the map, sometimes crashes at random and crashes while opening."
---

## Done when

At least one crash has a faulting module, exception code and the last lines of both logs, and
the mod is either cleared or identified as the cause.

## Steps

1. Find where the game's own crash handler writes dumps (`dumplevel` flag, `CrashGuard.cpp`), or
   turn on `veh_crashdumps=true` through the mod for one session. Check: a dump file appears after
   a crash.
2. Reproduce once with the mod, once with `dist/uninstall.ps1` applied. Check: same or different.
3. Read the Windows Application event log entries for the exe. Check: faulting module recorded.

## Notes

- The mod's timeline CSV is flushed every second, so the last line before a crash shows VRAM use
  and streaming activity at the moment it happened.
