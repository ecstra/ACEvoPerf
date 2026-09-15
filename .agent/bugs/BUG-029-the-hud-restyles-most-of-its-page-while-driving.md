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

## Fix

Absent. The UI probe (`[developer] ui_probe=1`) logs every restyle pass over 15 ms with the first changed
nodes, counts invalidations by element, and its page script matches slow frames with the class, style,
attribute and DOM changes before them, which names the trigger. Once named, the fix is the kind the responsive
UI already carries, a page fix or a narrowed rule.

## Verification

Absent.
