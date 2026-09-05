---
name: TODO-003-capture-crash-evidence
kind: todo
description: get a faulting module and the last log lines for one crash, and clear or blame the mod
updated: 2026-09-05
links: [BUG-004-crash-on-car-or-track-change, BUG-005-crash-on-startup]
status: open
by: owner
area: stability
born: 2026-09-05
done:
---

## What

"General Stability: Game crashes everytime i change my car or change the map, sometimes crashes at
random and crashes while opening."

## Done when

One crash has an exception code or hang record, the last lines of `acevo_perf.log`, the last
timeline line and the game log tail, and the same action was tried once with the original
`dstorage.dll` restored. Steps: turn on `veh_crashdumps=true` through the mod ini for one
session and find where `CrashGuard` writes, ask the owner whether a crash shows a dialog, freezes
or vanishes, read the Windows Application log after the next one.
