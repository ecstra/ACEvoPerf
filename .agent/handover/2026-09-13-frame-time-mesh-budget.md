---
name: handover-2026-09-13-frame-time-mesh-budget
kind: doc
description: state of the frame time work after runs N, M and P1 to P3, paused to measure release 0.3.1 against it
updated: 2026-09-14
links: [TODO-022-frame-time-with-and-without-the-mod, frame-time-mod-against-passive-2026-09-13, texture-streamer-flip-2026-09-13, DEC-005-fixed-pool-sizes-by-default, mesh-level-of-detail-2026-09-14]
---

# Handover: frame time, the mesh budget

Superseded on 2026-09-14 by the mesh deep dive,
[mesh-level-of-detail-2026-09-14](../docs/research/mesh-level-of-detail-2026-09-14.md). The 0.3.1 run
was done, the branch's commits are on main, and the race line below is corrected there, the 56 fps
race is confounded and not a pair.

## Where the work stands

- Branch `fix/frame-time-gap`, pushed, everything committed. Main holds the reload fix branch,
  merged and deleted.
- The research doc [frame-time-mod-against-passive-2026-09-13](../docs/research/frame-time-mod-against-passive-2026-09-13.md)
  has every run. The mod costs 3.8 percent parked and 8 percent on a lap against itself passive.
  Most of that is the mesh detail the canonical 1433 MB mesh budget loads. The texture pool's cost
  is inside the drift between sessions, and the rest of the mod is about 1 percent.
- On the branch: `[developer] mesh_budget_mb`, a stub on the engine's mesh budget function, off by
  default, and the exe patch helpers moved into `src/engine/code_patch.cpp` for it.
- A 366 MB mesh budget gets the frames back unseen, but starves the mesh streamer, frozen on the GP
  and churning 1.6 to 2.2 GB of uploads a lap at the Red Bull Ring. No budget size is a fix.
- Paused here to run release 0.3.1 on the same protocol, the owner asking whether 0.3.1 had the
  gap at all. 0.3.1 has the same settings as run M except the reload fix and the big screens fix.

## Verified against written

- Verified by logs and the owner's eye: runs N, M, P1, P2 and P3 with their windows and pools, the
  mesh budget patch applying, the churn at the Red Bull Ring against the ten laps of
  `logs/streamer-boot1-1124`.
- Not a pair, so open: the 29 AI race ran CPU bound at 56 fps with 366 MB against 67 fps at 1433 MB,
  at 8:00 against 15:00.
- Untested: Reflex, the priorities and the bundled runtime, each on its own.

## Next

1. The 0.3.1 run, parked and one lap at the Nürburgring GP, on the research doc's windows, 100 s from
   20 s after the HUD and from 6 s after leaving the pit box to the lap completing.
2. Restore the install from `logs/install-snapshot-20260913-1545`, the branch build with the
   developer CSVs on and `mesh_budget_mb=0`.
3. The owner's call between a deep dive into how the mesh streamer picks its levels, the only lead
   that could be a real fix, and closing TODO-022 as detail.

## Traps

- The game folder ini is whatever the last run needed. Diff it against the run's saved ini before
  every launch.
- Release 0.3.1 reads `timeline` and `frames` from `[log]`, the branch from `[developer]`.
- Sessions drift by about 0.6 fps parked, so a single run against an old one cannot resolve less.
- The nvidia-smi sampler's first rows after starting can read `[Unknown Error]`, skip them.
