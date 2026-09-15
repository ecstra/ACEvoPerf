---
name: TODO-010-resume-the-one-percent-low-hunt
kind: todo
description: pick the 1 percent low hunt back up from the recorded evidence, now through the deep dive of 2026-09-14, whose two runs replace the leads never tested and the instruments that were removed
updated: 2026-09-15
links: [BUG-009-one-percent-lows-far-below-average, BUG-013-one-percent-lows-drop-after-window-or-input-switch, one-percent-low-hunt-2026-09-05, one-percent-lows-2026-09-14, TODO-025-the-ui-view-rotation-test, TODO-026-one-lean-etw-trace-of-the-slow-frames, telemetry, reference-machine-has-no-direct-gpu-display]
status: open
area: render
created: 2026-09-06
done-when: a clean driving minute has its slowest 1 percent of frames within 1.25 times the median, or the cause of the late main batch is named with a measurement and a fix in the mod or a documented setting
---

## Ask

Owner wording, 2026-09-06: "either fix or remove all logging and mark it as planned for future."
The logging is removed, this is the plan.

## Where it stands, 2026-09-14

The deep dive of 2026-09-14 read the nineteen laps again with the exe
([one-percent-lows-2026-09-14](../docs/research/one-percent-lows-2026-09-14.md)). The slowest frames are
the present path through the integrated GPU, coupled to the previous frame's GPU end, then 1.2 to 2.8 ms
of spread out renderer code, then periodic pieces led by the game advancing one UI view per frame in
rotation. Reflex already evens the long short alternation. No fix is shown to narrow the width on the
Nürburgring protocol.

## The leads, answered or moved

- **Displays on the discrete GPU.** Not possible on the reference machine, no MUX and no cable, see
  [reference-machine-has-no-direct-gpu-display](../memory/reference-machine-has-no-direct-gpu-display.md).
  Answered on 2026-09-15 by the owner's 5070 desktop, which has no integrated GPU and shows the same gap,
  so the integrated GPU is not the cause (BUG-009). The UI view rotation goes first.
- **Which job the render thread waits for.** Answered, no lock and no job. Lock waits add 0.01 to 0.22 ms
  per slow frame and the scheduler about 0.06 ms, the extra waiting is the present call and the frame
  latency object.
- **HUD script on the render thread.** Answered, V8 comes in bursts after a load and does not set the
  width. The UI's weight comes through the view rotation instead, tested by
  [TODO-025](TODO-025-the-ui-view-rotation-test.md).
- **Render thread isolation.** No processor shortage was found, and lap 18's slower core was the CPU clock.
  Waits on [TODO-026](TODO-026-one-lean-etw-trace-of-the-slow-frames.md) showing ready time or a busy
  sibling.
- **The 0.1 percent.** File to memory requests on the render thread's frame, a streaming hitch, not the
  spread. Unchanged.

## Instruments

None come back. One lean ETW trace started from the owner's elevated prompt sees what the in process
sampler, wait hooks, fence hooks and core speed probe saw, without moving the present path the way the
sampler did (TODO-026).

## The done-when needs the owner

The line above cannot tell a fix from a lucky minute. Read as p99 over median, single minutes on disk
already cross 1.25 (R's lap minute 2 at 1.221), and read as the slowest 1 percent's mean over median whole
laps sit at 1.308 to 1.391. A measure that works compares a stint against a baseline stint inside the same
launch with a fixed margin, using whatever statistic the owner's frame counter shows. Kept as written until
the owner picks.
