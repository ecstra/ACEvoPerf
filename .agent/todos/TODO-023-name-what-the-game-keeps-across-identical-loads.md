---
name: TODO-023-name-what-the-game-keeps-across-identical-loads
kind: todo
description: one run with the memory census back in a developer build, six identical Nürburgring GP visits and two Task Manager memory dumps, to say whether the game's live heap keeps growing on identical loads and name what grows
updated: 2026-09-14
links: [BUG-016-vram-overhead-grows-across-scene-loads, memory-creep-2026-09-14, TODO-022-frame-time-with-and-without-the-mod, telemetry]
status: open
by: agent
area: stability
born: 2026-09-14
done:
---

## What

Run the BUG-016 repeat route with the memory census and two memory dumps, from the deep dive of
2026-09-14 ([memory-creep-2026-09-14](../docs/research/memory-creep-2026-09-14.md)).

Build. A developer branch that brings the memory census back exactly as the S session ran it (commit
`0770d67` holds it, `8450d2e` removed it), settle 15 s within 150 MB, a reading at start and after
each fall of 700 MB or more from the peak. Nothing else changes. `timeline=1` and the census switch
on, `streaming_trace=1` may ride along. No `HeapOptimizeResources`, it would change the slack the dumps
measure. Built before the owner sits down.

The owner's route, about 20 minutes plus the dump freezes.

1. Launch, wait 45 s in the menu without touching anything.
2. Practice at the Nürburgring GP with the Ferrari 296 GT3, the same weather and time as S, parked in
   the pit box for 60 s.
3. Exit to the menu and wait 45 s.
4. After this first return only, Task Manager, Details, right click `AssettoCorsaEVO.exe`, Create
   memory dump file, wait for it, then move the file out of the temp folder to a folder outside the
   repo named M1.
5. Repeat steps 2 and 3 five more times, six visits in all.
6. After the sixth return wait 45 s and take the second dump, named M6.
7. Quit from the menu.

A dump is 11 to 12 GB, freezes the game for an estimated 15 s to a minute, and holds the whole process
memory, so it stays on the owner's disk and is never shared or committed.

Ride along for TODO-022, if the owner agrees: `-log_info=meshStreamer` in the Steam launch options for
this run, removed after it.

## Why

No session on disk has more than four track visits, commit alone hides live growth inside the heap's
held slack, and the same repeat step differs by 150 to 300 MB between runs. Only identical loads in one
run with a heap walk can tell a one time fill from a leak and name the owner.

## Done when

BUG-016 holds the outcome of the census steps (live heap flat within 30 MB from the second repeat, or
still growing), the M1 to M6 difference by C++ class and block size, the real heap slack at M1 and the
mesh pool block walk.
