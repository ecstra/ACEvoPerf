---
name: BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash
kind: bug
description: the streamer's periodic log line reads the engine's tile pool through an allocator pointer saved during a kick, after the game has freed it at exit, and although the mod catches the fault the game's own crash handler writes a crash report naming the mod's DLL into the game log
updated: 2026-09-13
links: [texture-streamer-overload-2026-09-13, telemetry, BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles]
status: open
severity: bug
area: stability
reported: 2026-09-13
parent: BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles
---

## Problem

Quitting the census race of 2026-09-13 left a crash report at the end of the game log, an access
violation on the thread `ACEvoPerf timeline` in `DSTORAGE.dll` at RVA 0x100A3, with a stack through
`StreamerTick`. The game did not crash, it went on to write the mod's exit lines, but anyone reading
that log, a user or Kunos, would take it for the mod crashing the game.

## Evidence

`logs/census-ai30-20260913/game_log.txt` at 19:58:36.022, and `acevo_perf.log` at 19:58:36.466 with
the `[streamer]` line ending `tile pool 0 of 0 used, 0 pending (not read yet)`. RVA 0x100A3 of that
build is `ReadPoolLine` in `src/engine/streamer.cpp` loading `[allocator+0xD2A8]`, where the
allocator pointer is the one `BeginEvent` stored on the last kick. The game's vectored handler logs
the fault before the `__try` in `ReadPoolLine` handles it. No other session in `logs/` has it, the
timeline tick only hits the window between the engine freeing the allocator and the process ending
by chance. Reading the pool inside the kick, where the allocator is alive, and keeping the figures
for the log line would keep the timeline thread off engine memory altogether.

## Fix

Absent.

## Verification

Absent.
