---
name: handled-faults-stall-the-game
kind: memory
description: the game's crash logger catches every access violation first, even one the mod's __try handles, and stalls that thread 120 to 210 ms writing a symbolised stack, so mod code must never probe memory by faulting
updated: 2026-09-15
links: [telemetry, BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash, responsive-ui-rounds-2026-09-15]
type: project
---

Assetto Corsa EVO 0.9.1 logs `[crash] Exception Detected` with a symbolised stack for every first chance
access violation in the process, before any structured exception handler runs. The UI probe read nodes
already taken out of the page inside `__try`, and each handled fault held a render worker or the game
thread for 120 to 210 ms, 15 times in one lap of 2026-09-15 and hundreds in earlier probe laps. The owner
felt them as UI stutter and they inflated the probe's own timings.

It matters because a fault the mod treats as harmless is a visible hitch in this game, and the game log
names `DSTORAGE.dll` with meaningless export names, so a stranger's crash report points at the mod.

Apply it by searching the game log for `Exception Detected` after any lap with new hook code, by checking
pointers through structure before reading them (a node's element and connected bits, sizes, build checks),
and by keeping `__try` as the last resort only.
