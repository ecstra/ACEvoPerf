---
name: TODO-010-resume-the-one-percent-low-hunt
kind: todo
description: pick the 1 percent low hunt back up from the recorded evidence, the leads that were never tested and the instruments that were removed
updated: 2026-09-06
links: [BUG-009-one-percent-lows-far-below-average, BUG-013-one-percent-lows-drop-after-window-or-input-switch, one-percent-low-hunt-2026-09-05, telemetry]
status: open
area: render
created: 2026-09-06
done-when: a clean driving minute has its slowest 1 percent of frames within 1.25 times the median, or the cause of the late main batch is named with a measurement and a fix in the mod or a documented setting
---

## Ask

Owner wording, 2026-09-06: "either fix or remove all logging and mark it as planned for future."
The logging is removed, this is the plan.

## Where it stands

Nineteen laps say the slowest 1 percent of frames are the render thread handing its main
command list batch to the GPU 3 to 5 ms late in heavy views, with the GPU idle in that gap, the
work spread across the renderer's code, and everything else ruled out. The full record is in
`one-percent-low-hunt-2026-09-05` and BUG-009.

## Leads never tested

- Displays on the discrete GPU. The game's log lists both monitors as outputs of the AMD
  integrated adapter and the render thread waits inside the AMD D3D11 driver every present, 0.9
  ms in a median frame and up to 4 ms in a slow one. A MUX, hybrid mode off, or a port wired to
  the RTX 3060 removes that path. The owner says the laptop has no Optimus, so first find out
  what the vendor tool offers.
- Render thread isolation: pin the render thread to one physical core and keep the game's other
  threads off its hyperthread sibling, raise it above the workers. The core speed probe said the
  core was only 5 percent slower in slow frames, so this is a small lever, but it is the one
  the mod can pull without touching game data.
- Which job the render thread waits for. The frame's main batch is late because the thread
  spends its extra time in the renderer's job scheduler helping and waiting. A per frame count
  of the job counters it waited on, with the counter's owner named from the exe, would say which
  system is late (culling, animation, physics sync, UI).
- HUD script on the render thread. V8 runs there only in slow frames, 0.3 to 1.2 ms. UI work is
  off limits until the owner explains the UI problem, but the finding belongs to that talk.
- The 0.1 percent: file to memory requests on the render thread's frame (5 frames of 45 ms in
  lap 16, all with `f2m_req` above zero), a streaming hitch, not the spread.

## Instruments to bring back

All in the history before the commit `removed: the latency hunt instrumentation`: the render
thread sampler with the per minute slowest 1 percent cut and the stack module chain, the GPU
timestamp marks per command list batch, the wait and fence hooks, the tile mapping and submit
timing, the core speed probe, the input probe and the device watch. Each one takes a config key,
none of them belong in the shipped build.
