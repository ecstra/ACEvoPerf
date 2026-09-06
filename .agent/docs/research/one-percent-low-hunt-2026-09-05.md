---
name: one-percent-low-hunt-2026-09-05
kind: doc
description: nineteen instrumented laps into the gap between the average and the 1 percent low, what each one measured, what was ruled out and what remains
updated: 2026-09-06
links: [BUG-009-one-percent-lows-far-below-average, BUG-013-one-percent-lows-drop-after-window-or-input-switch, TODO-010-resume-the-one-percent-low-hunt, lap-2026-09-05-nordschleife, telemetry]
---

# The 1 percent low hunt, 2026-09-05 to 2026-09-06

The owner's complaint: the average sits at 80 to 90 fps and the 1 percent low 20 to 25 fps
under it, on every lap, and the gap does not move with settings. This is the record of the
evening and morning spent on it, so the next attempt starts from evidence.

## Method

One clean lap from the pit exit per build, the same stretch every time, the machine idle while
the owner drives. The report cuts a 45 or 60 second window and compares the slowest 1 percent
of frames with the faster half. Every instrument lived behind a config key and was removed at
the end, the code is in the history before `removed: the latency hunt instrumentation`.

Reference machine: RTX 3060 Laptop 6 GB rendering, AMD Radeon integrated graphics owning both
displays (300 Hz internal panel, 165 Hz Lenovo G24-20), 16 logical cores, game 0.9.0+release.48
windowed 1920x1080 with DLSS, vsync off, the mod's fixed pools and 128 MB staging buffer.

## The laps

| lap | instrument or change | avg fps | 1% low | slowest 1% mean | median | finding |
| --- | --- | --- | --- | --- | --- | --- |
| 9 | Present call timed | | | | | none of the slowest 42 frames spent half their time in Present |
| 11 | render thread sampler, buckets by module and nearest system export | 88 | 60 | | | slow frames carry extra game code and extra kernel waits, cohtml under 1 % |
| 12 | every wait of the render thread hooked in every module, fences told apart | 89.5 | 57.5 | 17.4 ms | 11.1 ms | fence waits zero, hooked waits 0.6 ms, present 2.0 ms, the sampler's waits are not handle waits |
| 13 | stack scan under each outside sample | 86.5 | 59.5 | | 11.4 ms | the waits are `ZwWaitForAlertByThreadId` (SRW, condition variables), the scheduler's callee that looked like a sleep is `operator new` |
| 14 | sampler cut per minute, slowest 1 % against the median half | 84.3 | 60.4 | 16.6 ms | 11.9 ms | game code flat, the extra is `NtWaitForSingleObject` under a non game module, 17 samples against 1.8 |
| 15 | tile mappings and command list submits timed | 84.9 | 59.3 | 16.9 ms | 11.4 ms | mappings in 9.8 % of the slowest frames against 1.7 %, 0.08 ms per call, submits 0.78 against 0.48 ms |
| 16 | GPU timestamps around every batch on the present queue | 82.7 | 57.0 | 17.5 ms | 11.8 ms | GPU busy 13.1 against 10.3 ms, span 16.6 against 10.9, busy autocorrelation 0.94 at every lag |
| 17 | frame latency 3 | 84.9 | 61.4 | 16.3 ms | 11.6 ms | per batch: the GPU idles 4.2 ms before the main batch against 0.8, the batch starts 0.16 ms after its submit |
| 18 | render thread core speed per frame, sampler off | 84.6 | 59.3 | 16.9 ms | 11.7 ms | core 1950 against 2056 loop iterations per us, CPU at 118 to 126 % of nominal all minute |
| 19 | car reflections Low, shadows Low, LOD Medium, vehicle LOD Medium, mirror off, grass High | 96.3 | 53.9 | 18.5 ms | 10.3 ms | GPU work flat at 9.9 against 9.1 ms, the gap before the main batch is the whole excess, p99 over median 1.29 |

The 1 percent low is the mean of the slowest 1 percent, so a handful of 45 ms streaming hitches
in a window pull it down (lap 19, 0.1 percent low 17.7 fps). The p99 over median ratio is the
cleaner number for the spread: 1.26 to 1.38 in every lap.

## What the slow frames are

The game submits four command list batches per frame, the second is the whole frame's rendering.
In a median frame the GPU finishes the previous frame and starts the main batch 0.8 ms later. In
the slowest 1 percent it waits 3 to 5 ms for that batch, and the batch begins within 0.2 ms of
the moment the CPU submits it. The render thread is late, the GPU is starved.

The render thread's extra time in those frames, from the sampler's driving minute cut:

- game code, about 3 ms, spread over the renderer's draw recording with no hot spot, the top
  entry 19 samples against 11 expected
- the present path, 0.9 ms in a median frame and up to 4 ms in a slow one, on the stack as
  `dxgi > d3d11 > atidxx64`: the frame is copied to the integrated GPU for display
- HUD script (v8), 0.3 to 1.2 ms, present only in slow frames
- driver, 0.6 ms

The GPU's own work per frame is smooth, autocorrelation 0.94 at every lag, drifting with the
track section and never spiking. The slow frames sit in heavy sections, where the GPU needs 13
ms and the CPU side arrives late on top of it.

## Ruled out, with the lap that did it

- the swap chain queue and the Present call: 9, 12
- D3D12 fence waits: 12, none at all
- kernel wait handles: 12, one semaphore once per frame for a few hundredths of a millisecond
- a hot spot in the game's code: 11 to 14, the scheduler spin lock that led every cumulative
  list was the loading phase, the driving minute is flat
- the refresh rate or the compositor: 16, one smooth hump from 9 to 15 ms, no peaks at 3.33 or
  6.06 ms multiples, vsync off throughout
- GPU decompression: the game streams raw data, zero compressed requests
- tile mappings: 15, in more of the slowest frames but 0.08 ms each and in 5 of 51
- the GPU clock and temperature: 16 to 19, 1660 to 1900 MHz, the spread ratio is the same at
  every clock
- the CPU clock and the render thread's core: 18
- the sampler's own load: 18, same numbers with it off
- GI, the dynamic track, the GPU heavy settings: 2026-09-05 22:00 runs, no change to the ratio
- the CPU heavy draw settings: 19, the average rose 12 fps, the ratio moved from 1.34 to 1.29
- frame latency: 1 halves the frame rate (DEC-008), 3 gave 61 against 57 on one lap and 59 on
  the next

## What remains

TODO-010 holds the leads: the displays on the discrete GPU, render thread isolation, naming the
job the render thread waits for, the HUD script timer. None of them has a measurement yet.
