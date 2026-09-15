---
name: BUG-029-the-hud-restyles-most-of-its-page-while-driving
kind: bug
description: while driving, the HUD now and then restyles a large part of its page on the UI worker, about 24 ms of Cohtml style matching, and the game thread waits for it in EvoUi's end of frame, so one frame takes 35 ms and a short window 1 percent low reads about 30 fps
updated: 2026-09-15
links: [BUG-009-one-percent-lows-far-below-average, BUG-028-page-opens-still-hold-frames-of-100-to-200-ms, responsive-ui, TODO-026-one-lean-etw-trace-of-the-slow-frames]
status: open
severity: bug
area: ui
reported: 2026-09-15
parent: BUG-009-one-percent-lows-far-below-average
---

## Problem

In the owner's words about the NVIDIA overlay while driving, "the 1% was dropping to like 30fps at times", and on
the traced lap "it dropped twice but only for an instant". A single long frame does that. One 35 ms frame among
100 fps frames reads as a 28.5 fps 1 percent low over the second around it.

## Evidence

`logs/trace-long-20260915`, the lean trace (`tools/data/acevo_frames.wprp`) over an out lap and the start of lap
one at the Red Bull Ring GP in the Ferrari 296 GT3, build `2483f0b` with the responsive UI and the HUD every
frame. In that session the exe loaded at `0x7FF6F4230000`, Cohtml 1.61.0.3 at `0x7FFDB4B20000`, V8 at
`0x7FFCEFE00000` and the mod at `0x7FFDB6000000`. Of 14,032 driving frames three were over 16 ms, one of them
35.0 ms at 19:26:30.9, trace time 100.8548 to 100.8899 s.

- **The game thread does not wait on the GPU or the compositor in that frame.** After `Present` returns at
  100.8577 s it runs almost without a break for 32 ms, preempted only for microseconds. The next `Present`
  returns in 0.4 ms, so the GPU sat waiting.
- **It spends the frame waiting for the UI job.** Of its 32 samples, 2 are the mod's Reflex call right after
  `Present`, and from about 10 to 34 ms into the frame the stacks run from the frame step (`0x896623`) through the
  mod's EvoUi end of frame hook into EvoUi's end of frame (`0xDE75C0`) and the job counter wait (`0x27A15B0`),
  spinning on the scheduler's lock (`0x279FA90`, `0x27A1550`).
- **The UI worker restyles the HUD page.** Thread 303840, the one that readies the game thread when the UI job
  ends, has 28 samples in that window. They run from the UI job (`0xDD7940`) into `View::Advance` (`0xDE2677`)
  and Cohtml's style update (`0x4725B0`, `0x489E90`, `0x472F00`, `0x35C26A`, `0x3FCC40`, `0x3FA980`,
  `0x3FA740`) to the element style match (`0x3EF520`) and the rule matcher (`0x3EEE70`) with its selector checks
  (`0x3ED9D0`, `0x3EDC70`). 14 samples sit in the rule matcher and 16 in the style matching fix's stubs, a few in
  V8. The same restyle cost the responsive UI cut on menu pages, taking about 24 ms here.
- **The other drop is smaller and is script.** A 16.5 ms frame at 19:26:44.3 has the UI worker in V8 through
  Cohtml's bindings for about 2 ms while the game thread waits briefly in the job system.

What change on the HUD sets off the restyle is not in the trace.

`logs/probe-hud-20260915`, the UI probe over ten laps and a drive through the pit lane, same car, track and
build, `ui_probe=1`. Six driving frames of 29.4 to 32.5 ms carry 22.8 to 24.5 ms of UI end frame wait, and
each has one child list change (invalidation kind 0) on a `div.component-body` marking 878 nodes, then a
restyle of 20.8 to 23.0 ms over 879 nodes. The HUD's top level `.component-body` under `ks-hud` is the only
element with a subtree that size.

| Time | Where | Frame | UI end frame | Restyle |
| --- | --- | --- | --- | --- |
| 20:10:52.4 | pit lane, 12 s after entering | 30.2 ms | 23.8 ms | 21.4 ms |
| 20:10:59.1 | pit lane | 32.2 ms | 24.5 ms | 23.0 ms |
| 20:11:04.1 | pit lane, 4 s before leaving | 32.5 ms | 23.8 ms | 22.3 ms |
| 20:13:34.0 | lap 6 | 29.5 ms | 22.8 ms | 20.8 ms |
| 20:13:35.6 | lap 6 | 29.4 ms | 23.4 ms | 21.0 ms |
| 20:19:18.6 | lap 9 | 32.1 ms | 23.6 ms | 21.8 ms |

- **Kind 0 is a child list change.** `0x37B600` passes it with empty names, and `0x3F24B0` gives every kind 0
  the same invalidation set, which on the HUD matched every node under the element.
- **The page's scripts did not do it.** The page script's counters of `appendChild`, `insertBefore`,
  `removeChild` and `replaceChild` read 0 in those seconds while they counted about 20,000 other writes a
  second, so the change comes from Cohtml itself, most likely a `data-bind-if` on one of the top level's
  children (`#labelContinue`, `#labelDisconnected`, `#labelWrongWay`, `ks-laptime`, the two `ks-timer`,
  `ks-hudwidgethost`, `ks-hudchat`, `ks-freecam`).
- **The game log has nothing at those times.** The nearest lines are penalty cleared notices 2.3 s after the
  lap 6 pair and 8.8 s before the lap 9 change.

The same run had two stretches of 1.2 and 1.5 s of mostly 80 to 90 ms frames, at 20:13:01 on lap 6 and
20:21:07 on lap 10. Most of those frames wait on no UI work, and the UI calls caught inside them take 73 to
84 ms each, `Advance` on the HUD view in both, on a car display in the first, and a 71.6 ms restyle of one
node in the second. None of the five drives of 2026-09-15 without the probe shows one, so the probe itself
is the first suspect. The next run checks it.

## Fix

Absent. The UI probe now logs each change of the HUD's top level with the model values its conditions read
(`[ACEvoPerf] ui hud top level` in the game log) and the call stack of every child list change that marks
200 nodes or more (`[ui] big child list change`), and skips its change counters on the HUD. That names the
element and the binding. The fix then keeps that element's change from reaching the top level, or narrows
what a child list change marks.

## Verification

Absent.
