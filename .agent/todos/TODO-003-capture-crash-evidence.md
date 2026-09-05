---
name: TODO-003-capture-crash-evidence
kind: todo
description: get a faulting module and the last log lines for one crash, and clear or blame the mod
updated: 2026-09-05
links: [BUG-004-crash-on-car-or-track-change, BUG-005-crash-on-startup]
status: dropped
by: owner
area: stability
born: 2026-09-05
done: 2026-09-05
---

## What

"General Stability: Game crashes everytime i change my car or change the map, sometimes crashes at
random and crashes while opening."

## Done when

Dropped on 2026-09-05: the crashes stopped with the staging buffer cap before any evidence was
needed (BUG-004 and BUG-005 fixed, owner verified with repeated car and track changes). The
`veh_crashdumps` flag stays documented in the ini for the next time.
