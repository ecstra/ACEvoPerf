---
name: one-percent-lows-2026-09-14
kind: doc
description: BUG-009's deep dive with no new run, the slowest frames are the present path through the integrated GPU coupled to the previous frame's GPU end, spread out renderer code, and a phase locked ripple from the game updating one UI view per frame in rotation, not a lock or a job, with Reflex already evening the long short alternation, no processor shortage, and two runs that need no build
updated: 2026-09-14
links: [BUG-009-one-percent-lows-far-below-average, TODO-010-resume-the-one-percent-low-hunt, TODO-025-the-ui-view-rotation-test, TODO-026-one-lean-etw-trace-of-the-slow-frames, one-percent-low-hunt-2026-09-05, optimisation-deepdive-2026-09-12, reflex-2026-09-12, DEC-014-reflex-ships-on-boost-ships-off, ui-lag-deepdive-2026-09-14, telemetry, reference-machine-has-no-direct-gpu-display]
---

# The 1 percent lows, the deep dive

BUG-009's round of 2026-09-14. Five angles from the exe, the frames CSVs and the old instrumented laps,
each checked by a second agent, then a critic and two gap rounds. No new session. RVAs are for the
0.9.1 exe unless marked 0.9.0. Laps 8 to 22 are the instrumented 0.9.0 laps of
[one-percent-low-hunt-2026-09-05](one-percent-low-hunt-2026-09-05.md), and N, M, P1 to P3 and R the runs
of 2026-09-13. The agents' scripts lived in the session's scratch space and are not kept.

## What the slowest frames are made of

On the reference protocol (Ferrari 296 GT3, Nürburgring GP), biggest first.

1. **The present path.** At the game's frame latency of 2, the render thread's extra wait in a slow frame
   is the `Present` call (2.5 to 3.1 ms more) and the swap chain's frame latency object (0.3 to 0.9 ms
   more). `Present` carries 50 to 59 percent of the slow frames' excess on the laps without GPU marks.
   The path goes through the AMD integrated GPU and cannot be removed on this laptop.
2. **The render thread's own game code**, 1.2 to 2.8 ms more in slow frames, spread over the renderer
   with no hot spot, not named yet.
3. **Periodic pieces on top.** The UI view rotation, a 60.000 Hz beat the long frames lock to in some
   launches, and the texture streamer's pass.
4. **UI work on the render thread's own processor time.** V8 bursts after a load. Taking every UI sample
   out of the slow frames moves p99 over median by at most 0.006, so it does not set the width.

## The present path

- **Present returns a fixed time after the GPU finishes the previous frame.** The model "return = max
  of entry plus a floor, or the GPU end of frame k minus 1 plus C" reproduces the `Present` duration at a
  correlation of 0.85 to 0.96 on laps 17 to 19 (C 2.5 to 2.8 ms) and 0.86 to 0.92 on laps 9, 12 and 15
  (C 3.8 to 4.4 ms, the GPU marks shortened C by about 1.5 ms). So a heavier GPU stretch turns straight
  into a longer present. `Present` carries 60 to 62 percent of frame time variance. No lock of present
  returns to 60, 120, 144, 165 or 300 Hz.
- **The swap chain.** Flip discard, 3 buffers, flags 0x840, windowed 1920 by 1080, sync interval 0, on a
  display of the AMD adapter (exe `0x1E20910` to `0x1E20A7C` and the mod's logs).
- **The game waits on its frame latency object once a frame.** BeginFrame `0x1CE3A70` calls
  `0x1E3F8E0` at `0x1CE3AA6`, a `WaitForSingleObjectEx` with 1000 ms on the handle from
  `GetFrameLatencyWaitableObject`. The 0.9.0 wait tallies show one semaphore wait per frame, 0.3 to
  0.9 ms more in slow frames at latency 2 and none at latency 3.
- **Instruments inside the process move this path.** The old sampler shifted 0.5 to 0.7 ms between
  `Present` and the rest.

## No lock and no job

- **The lock waits are small.** `ZwWaitForAlertByThreadId`, under every SRW lock, condition variable,
  `std::mutex` and critical section, adds 0.01 to 0.22 ms per slow frame in the per minute cuts of laps
  14 to 17, and the job scheduler's spin and counter wait about 0.06 ms (lap 17). Present time and the
  kernel wait bucket correlate at 0.968 to 0.975 per frame, and fence waits are zero.
- **The 2026-09-12 table was the load.** Its summary at 23:02:06 is in lap 13's log, built from commit
  `ae7a34e`, whose tallies were never reset, so "cohtml plus 743" and "ZwWaitForAlertByThreadId 762
  against 369" include the session load. The minute ring and the 100 ms filter the re-verification
  quoted arrived in `ef9fd2b`, the lap 14 build. Lap 13's real second minute, the difference of its two
  summaries, gives cohtml minus 2.2, the wait plus 2.7 and v8 plus 40.9 samples.
- **The scheduler on 0.9.1** moved by exactly `0xE1F0`, the spin lock from `0x27ADC80` to `0x279FA90`,
  the unlock to `0x27A1550` and the counter wait to `0x27A15B0`, which runs any queued job and spins with
  no `pause` on an empty ring.

## The UI view rotation

- **The game advances and paints one Cohtml view per frame.** The HUD job builder `0xDE3650` skips its
  selection when `GameUi+0x555` or `+0x556` is set (tests at `0xDE379B` and `0xDE37A9`), otherwise it runs
  `idx = (idx + 1) mod n` (`0xDE37D1` to `0xDE37E0`) and cuts the job list to `list[idx]` (`0xDE37F3`).
  The list (`0xDE46A0`) is the HUD view, then the player car's dashboard display views whose enabled byte
  `+0x118` was set at car setup from the `NumDashDisplays` video setting (`0xDE734F`, All, MaxOne or
  MaxTwo). Every game log reads MaxTwo, so n is 3 for a car with two displays and 2 for the Mazda RX-7 FD.
  `View::Advance` (`0xDE2630`) and the paint (`0xDEE230`) act only on the chosen view. When the flags are
  set, and where, is in [ui-lag-deepdive-2026-09-14](ui-lag-deepdive-2026-09-14.md).
- **The display count predicts the cycle in every driving segment on disk.** 69 segments of two display
  cars (Ferrari 296 GT3, Lamborghini Huracán ST evo2, Porsche 992 GT3 Cup, Mercedes AMG GT2) show a 3
  frame component of 0.16 to 2.33 ms against shuffled controls of 0.03 to 0.32. 5 Mazda segments show a
  2 frame component of 1.59 to 3.07 ms and no 3 frame one. The cycle changes with the car inside one
  launch, and pause shows none.
- **How it reaches frame time** (laps 10 to 19 with the proven row alignment). On the heavy view's frame
  the render thread carries 0.05 to 0.2 ms more cohtml and the GPU a batch of 0.42 ms against 0.07 to
  0.085 in the other slots, the next frame is the shortest, and two frames later `Present` is 0.55 to
  0.75 ms longer, the longest slot in 66 of 68 blocks. Those two slots hold 77 to 84 percent of the
  slowest 1 percent against 67 by chance.
- **Its weight depends on car and track.** Evening the slot means out of 1200 frame driving blocks
  changes block p99 over median by:

| car and track | segments | raw p99 over median | change |
|---|---|---|---|
| Ferrari, Nürburgring GP | 10 | 1.188 to 1.315 | minus 0.003 to 0.016 |
| Ferrari, Red Bull Ring GP | 3 | 1.267 to 1.304 | minus 0.034 to 0.057 |
| Ferrari, Monza | | 1.253 | minus 0.041 |
| Ferrari, the two 30 car races | | 1.241 to 1.246 | minus 0.026 to 0.030 |
| Mazda, Red Bull Ring national | 5 | 1.237 to 1.396 | minus 0.035 to 0.107 |

- **Every view's image is copied every frame.** The paint side (`0xDEE400` to `0xDEE5CE`) transitions
  every active view's render target and, for views with byte `+0x31` and a texture at `+0x90`, records
  four barriers and a `CopyTextureRegion`, also on frames where that view did not paint.
- **`no_dash` acts at load.** Its reader sits in the per car display setup `0xCA2560`, called once at car
  finalize, so with it the rotation list is the HUD alone and the HUD then advances every frame.

## The UI on the render thread's processor time

V8 comes in bursts of 2 to 10 ms in 10 to 22 frames per 90 s, clustered in the first 30 to 45 s after a
load, inside a scheduler job the render thread picks up in its counter wait. Removing every v8, cohtml
and renoir sample from the slow frames moves p99 over median by 0.000 to 0.006 on laps 10 to 17. So the
"HUD script on the render thread only in slow frames" lead is answered, it is not the width.

## Reflex evens the alternation

Over the four Reflex runs of 2026-09-12 (t 40 to 280 s), with Reflex off the frame to next frame
correlation is minus 0.63 after detrending, 13.3 percent of frames are under 0.85 of the median, and p99
over a 101 frame local median is 1.248. With Reflex on those read minus 0.25 to minus 0.42, 7.1 to 8.2
percent and 1.153 to 1.195. The global ratio barely differs because the GPU clock falls through those
laps. The runs of 2026-09-13 agree, N passive minus 0.86 parked and 1.309 local, M with the mod minus
0.67 and 1.255. Single launches on a throttling card, so the size carries medium confidence. The
alternation is damped rather than gone.

## No processor shortage

- The game uses 3.8 to 4.0 of 16 logical processors and the system under 6. Other processes' load moves
  the mean frame by 0.03 to 0.09 ms per busy logical processor and does not predict slow frames.
- The engine gives every thread an all ones affinity mask (`0xE5462E`), sets no thread priority, and the
  Physics thread busy waits one processor (`0x14D6870` to `0x14D6BBE`). CPU sets survive the engine's
  affinity call on this machine.
- Lap 18's "core 5 percent slower in the slowest frames" is one streaming stretch of 12 s where the CPU
  clock fell (119.8 against 125.0 percent of nominal), not the render thread's core.
- The mod's per second timeline tick is not aligned with slow frames (Rayleigh p 0.20 to 0.80).

## Other periodic pieces

- **A 60.000 Hz beat.** In some launches the long frames lock to a 60 Hz clock, present in 51 to 100
  percent of 30 s blocks in N, M, P1, P3 and Q, absent in P2, R and both races. Its phase holds within a
  session and jumps at loads, it is not either monitor's refresh, and removing it barely moves p99 over
  median (M parked 1.258 to 1.250). Source unnamed. The exe runs a 1 ms `timeSetEvent` next to
  `FSThreadOutputHandler::Run` (`0x2DFE9C8`).
- **The texture streamer's pass** adds about 1.0 ms over the pass frame and the next (27.1 percent of the
  slowest 1 percent fall in the 100 ms after a pass against 10 percent of frames, reload fix off). With the
  fix 0.91 ms in the fix1 race, 0.59 ms parked.
- **Parked, a slow frame is paid back by the next**, 95 to 100 percent of the slowest 1 percent are
  followed by a frame under the median. On laps slow frames sit in slow stretches.

## The measure

TODO-010's done-when, the slowest 1 percent within 1.25 times the median over a clean minute, cannot tell
a fix from a lucky minute. Read as p99 over median, minutes on disk already cross it (R's lap minute 2
1.221, the whole R lap 1.246). Read as the slowest 1 percent's mean over median, whole laps are 1.308 to
1.391. A measure that works has to be inside one launch against a baseline stint of the same launch.

## Candidates

None is shown to narrow the width on the reference protocol.

1. **A steady UI view schedule while driving.** Take the existing all views branch in `0xDE3650`, or keep
   the HUD every frame and rotate only the dashboards. Expected 0.003 to 0.016 narrower at the
   Nürburgring, 0.03 to 0.06 at the Red Bull Ring GP, Monza and in races, up to 0.107 for one display
   cars, for more UI work every frame (perhaps 0.25 ms of GPU if the HUD is the heavy view). A trade, not
   an engine bug, and part of the UI work.
2. **Skip the per frame target transitions and copy of views that did not paint.** At most about 0.07 ms
   a frame, a real waste fix only if the copy source is unchanged between visits. Needs the `+0x31` and
   `+0x90` fields read first.
3. **The present thread and Physics on CPU sets of their own.** At most 0.3 ms on slow frames, likely
   less. Only if TODO-026's trace shows ready time or a busy sibling.
4. **Present from a thread of the mod's own.** The model says the average rises and the width does not
   narrow, and Reflex already damps what it would remove. Only if the trace shows the latency object is
   released at present return rather than at GPU completion.
5. **The timer resolution back to the game's own 1 ms.** No measured benefit of the mod's 500 µs. Only if
   the trace shows idle worker wakes landing in slow frames.

## Runs

1. **The view rotation test, no build**, [TODO-025](../../todos/TODO-025-the-ui-view-rotation-test.md).
   Ferrari at the Red Bull Ring GP, the largest single car cycle. One launch stepping `NumDashDisplays`
   through MaxTwo, MaxOne, All and MaxTwo with a session restart after each, and one launch with
   `no_dash=true`. It proves the mechanism if the cycle follows 3, 2 and 4, names the heavy view from the
   mean frame time, and measures what a steady schedule buys.
2. **One lean ETW trace**, [TODO-026](../../todos/TODO-026-one-lean-etw-trace-of-the-slow-frames.md). It
   needs the owner's elevated PowerShell, the agents cannot record kernel events here (`xperf` access
   denied, `wpr` 0xc5585011). It names what the present thread waits on in slow frames and who wakes it,
   ready time and the SMT sibling, idle worker wakes, the 1.2 to 2.8 ms of game code by RVA from 1 kHz
   samples, and the 60 Hz source. The Windows Performance Toolkit is installed and its decoder prints
   context switch rows with stacks.

## What it corrects

- BUG-009 credited the alternation to reflection cubemaps, clouds and GI probes. The phase locked cycles
  are the view rotation, and lap 19 with reflections Low and the mirror off still shows the 3 frame one.
- BUG-009 treated `Present` as ruled out because no slow frame spent over half its time inside it. That
  rule hid that `Present` carries 50 to 59 percent of the slow frame excess through its coupling.
- BUG-009 said the game does not wait on the frame latency object. It waits once a frame at `0x1CE3AA6`.
- BUG-009 said "about 3 ms of game code" and "0.3 to 1.2 ms of HUD script only in slow frames". Game code
  is 1.2 to 2.8 ms on laps without GPU marks, and V8 comes in bursts after a load.
- The 2026-09-12 re-verification in
  [optimisation-deepdive-2026-09-12](optimisation-deepdive-2026-09-12.md) read lap 13's summary with the
  wrong sampler build, so cohtml is not the biggest contributor to slow driving frames.
- [moddability](moddability.md) says `WinPixEventRuntime.dll` and `OptickCore.dll` are plain imports of
  the exe. The exe references neither, only the DirectStorage cores name the PIX runtime.
- The `[display]` warning in `src/render/adapter.cpp` says "about a millisecond per frame". The 0.9.0 laps
  say 2.4 to 3.3 ms median per present, coupled to the previous frame's GPU end.

## Still open

- Which view is heavy, the HUD or the Ferrari's main display. The HUD fits better (the Mazda's display
  page binds only odometer, trip and clock yet its cycle is the largest, and the batch sizes match the
  list order HUD, 1024 square display, 512 by 128 display).
- Why the cycle weighs 0.43 to 0.85 ms at the Nürburgring but 1.65 to 2.02 ms at the Red Bull Ring GP and
  Monza, and why the Mazda's 2 frame cycle is larger than the Ferrari's 3 frame one.
- What the render thread's game code excess is, what C is made of across the two adapters, and the 60 Hz
  source. TODO-026 answers all three.
- Which frame counter the owner reads the 1 percent low from, and so which statistic the new done-when
  should use.
