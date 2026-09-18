---
name: BUG-032-the-game-freezes-at-a-thirty-ai-race-start
kind: bug
description: at the start of a thirty AI race at the Nürburgring the game stopped presenting frames for 45 s with its own threads frozen while the mod's threads kept running, no exception anywhere and video memory over budget, and the owner ended it
updated: 2026-09-18
links: [BUG-016-vram-overhead-grows-across-scene-loads, session-leak-fix, BUG-002-fps-drop-entering-new-track-sections, telemetry]
status: open
severity: bug
area: render
reported: 2026-09-18
---

## Problem

Owner wording, 2026-09-18, after a play session that ended with a thirty AI race at the Nürburgring, "it
crashed at ai race here". The game stopped at the race start and never came back, and the owner ended it.

## Evidence

`logs/airace-freeze-20260918/game_log.txt`, the mod's log and CSVs of that launch were overwritten by the
next one and only what was read on the day is kept here.

- **It was a freeze, not a crash.** Frames were presented at 60 fps with 16 ms frames until 17:55:22.9 and
  then stopped dead. The process stayed alive to 17:56:07 using about a quarter of the processor with no
  frames, and the mod's own timeline thread wrote its `[streamer]` line every ten seconds through all of it.
- **Nothing was logged.** The game log stops mid line at 17:55:22.534, with no exception and no crash report,
  and Windows recorded no application error and no display reset.
- **It froze inside the race start.** The last lines are the game giving all thirty cars their fuel and
  marking them as started.
- **Video memory was over budget.** 5416 MB used against a 5226 MB budget in the last report before it.
- **Not the memory census.** The census runs on the mod's timeline thread, which kept its ten second cadence
  through the freeze, so no census was in progress. A census at the next race start did freeze the game for
  13 s, which is a different thing and is why the census is off in a race.
- **Not the session leak fix.** Its last free was 76 s earlier in the menu, the race session was never freed,
  and the race at the next attempt froze on the census with nothing freed at all in that launch.

## Reproduce

Not reproduced yet. The second attempt the same evening ran the same race with the same build and did not
freeze, so it needs the census off, the same thirty AI grid at the Nürburgring, and a Task Manager dump taken
while the game is frozen.

## Fix

Absent.

## Verification

Absent.
