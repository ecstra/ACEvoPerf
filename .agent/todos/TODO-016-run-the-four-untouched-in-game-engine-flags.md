---
name: TODO-016-run-the-four-untouched-in-game-engine-flags
kind: todo
description: four of the 178 never run engine flags bear on frame rate and the 1 percent lows, and all four are reachable from the ini today with no build, so measure them from a fixed stationary view before spending effort on anything expensive
updated: 2026-09-12
links: [engine-flags, optimisation-deepdive-2026-09-12, BUG-009-one-percent-lows-far-below-average, BUG-002-fps-drop-entering-new-track-sections]
status: open
by: owner
area: engine-flags
born: 2026-09-12
done:
---

## What

Owner wording, 2026-09-12: "we'll start with cheap and easier wins first (agility SDK, VRS) and
then go to harder debugging ones. A is first since its the cheapest."

A is the flag batch. The deep dive found 178 of the 216 engine flags have never been mentioned
anywhere in this repo. Four of them bear on things that are still open, and the owner has ruled
that only in game work counts now, not load time.

| flag | type, default | what it should answer |
|---|---|---|
| `no_gi` | bool, off | what global illumination costs in frame time, as a ceiling. If it is large then `gibake_probes_per_frame` becomes a real lever, if it is small the whole GI angle closes |
| `log_pso_on_creation` | bool, off | whether pipeline state creation happens **while driving**, which is the shape of BUG-009 and of the community complaint that the first lap at each circuit stutters |
| `ai_run_dynamic_track` | bool, on | Kunos's own help text says "may be a performance issue" |
| `car_update_complete_max_interval` | int32, 20 | frames a car may go without a complete graphics update, a tail latency knob above the two budgets the repo already documents |

## Why

No build is needed. Any bool, int32 or double name from `tools/data/gflags_full.tsv` works in the
ini's `[flags]` section, so this costs one session and no code. It is the only item on the
current list that could put a frame rate number in front of us before committing to anything
expensive, and two of the four close permanently whichever way they go.

## How to measure, which is the hard part

The hazard recorded in the deep dive applies to all of it: the only signal available is
`frame_ms`, there is no GPU busy column in `acevo_perf_frames.csv`, and this laptop's clocks
slide from 1705 to 1512 MHz as it heats. A lap cannot resolve any of these.

So every pair runs **from a fixed stationary view, back to back, at the same thermal state**.
Park at the same spot, same car, same time of day, same weather, cockpit view, hands off, sixty
seconds. Two runs that differ only in the flag.

`[log] frames=1` and `timeline=1` for the runs, back to 0 afterwards.

## The two that need a grid

`ai_run_dynamic_track` and `car_update_complete_max_interval` both act on cars other than the one
being driven. In a solo hotlap they are inert by construction. They are only worth a run in a
session with a full grid, so they wait until the owner says he races one.

## Done when

Each of the four has a number or a recorded reason it cannot have one, written into
`.agent/docs/research/`, and `no_gi` has either opened the GI angle or closed it.

## Where it stands, 2026-09-12

Three parked runs, all written up in
[engine-flags-in-game-2026-09-12](../docs/research/engine-flags-in-game-2026-09-12.md).

**Two of the four are closed.**

- `no_gi`: global illumination costs **3.2 percent of frame time and 416 MB of video memory**,
  measured over 119 aligned parked seconds, slower in only 3 of them. Then
  `gibake_probes_per_frame=2` against its default of 16 recovered **none** of it and moved video
  memory by one megabyte, so the cost is the shading lookup and the probe storage rather than the
  per frame baking. Those are the only two GI flags in the engine, one cannot ship and the other
  does nothing, so the angle is closed.
- `log_pso_on_creation`: written correctly at both passes and emits nothing in the release build.
  A dead diagnostic. The question it was meant to answer still has no instrument.

**Two wait for a grid.** `ai_run_dynamic_track` and `car_update_complete_max_interval` are inert
in a solo hotlap.

This todo stays open only for those two.
