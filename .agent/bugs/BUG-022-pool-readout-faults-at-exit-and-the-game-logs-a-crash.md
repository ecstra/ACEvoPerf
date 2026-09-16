---
name: BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash
kind: bug
description: the streamer's periodic log line reads the engine's tile pool through an allocator pointer saved during a kick, after the game has freed it at exit, and although the mod catches the fault the game's own crash handler writes a crash report naming the mod's DLL into the game log
updated: 2026-09-16
links: [texture-streamer-overload-2026-09-13, telemetry, BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles, TODO-023-name-what-the-game-keeps-across-identical-loads]
status: fixed
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

On `fix/pool-readout-at-exit`, commit `a92be48`, 2026-09-16. The first hook event of each kick reads
the tile pool while the allocator is alive, through the same read the reload fix's margin test uses,
and keeps used, capacity and pending in atomics. The `[streamer]` line and its at exit copy print those,
so the timeline thread reads no engine memory at all. The saved allocator pointer and the timeline
thread's `__try` read are gone. The figures in the line are now the last kick's, about a second old
while the streamer runs, and `(not read yet)` still means no kick has read them.

## Verification

The census run of TODO-023 on 2026-09-16 (`logs/census-rbr-20260916`) carried this fix for 23 minutes
and fifteen scene loads. The `[streamer]` line printed real pool figures the whole session, 15,360 of
16,384 used on track, the line at exit read 11,689 of 16,384 used and 0 pending, and the game log holds
no crash report. The fault only ever came by chance at exit, so the clean quit shows the line still
works, and the fault is gone by construction, the timeline thread no longer reads engine memory.
