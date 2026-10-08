---
name: BUG-036-textures-resolve-in-visible-steps-and-a-mid-lap-restart-makes-it-worse
kind: bug
description: textures come up mushy and sharpen in visible stages, and a mid lap restart was much slower on the first run, not seen again on the 0.3.2 release or plain 0.4, and an admission fix for the streamer's double promise looked blurrier, so it stays as the engine has it
updated: 2026-10-08
links: [directstorage-streaming, BUG-018-whole-scene-low-detail-for-a-second-after-load, BUG-010-texture-pool-shrinks-on-race-load-and-restart, BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring, BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles, texture-streamer-camera-cuts-2026-09-14]
area: streaming
status: wontfix
severity: bug
reported: 2026-09-20
parent:
---

## Problem

Owner, 2026-09-20: "The textures are mushy and blurry at the start, then it becomes less blurrier
and then less blurrier before finally loading. (worsens or slows on session restart mid lap)."

Two parts, and the second is the interesting one.

The staircase: the detail does not arrive in one step. The scene starts mushy, sharpens part of
the way, sharpens again, and only then settles. That is the mip chain being handed over level by
level with each level held long enough to see.

The restart: doing a session restart from mid lap makes it worse or slower than the same scene
loaded fresh. A restart reuses a process that has already been driving, so whatever state it
starts from is not the state a fresh load starts from.

## Evidence

### Why it is filed separately from BUG-018

BUG-018 is the whole scene at low detail for one or two seconds after the curtain lifts, at game
start and at track entry, settling on its own. This is a different shape in both halves. The
staircase is several visible levels rather than one coarse moment, and the trigger that makes it
worse is a mid lap restart, which BUG-018 does not cover at all.

They may turn out to be the same mechanism seen from two angles, in which case one of them folds
into the other. Filing it separately keeps the restart evidence from being lost inside a record
that does not mention restarts.

### What it probably touches

- BUG-010 measured the engine resizing the texture tile pool during scene transitions, 633 MB in a
  race and 526 MB after a restart, and was fixed by the fixed pool size. A restart being worse is
  the same trigger, so whether anything of that survives the fix is the first thing to check.
- The camera cut work of BUG-021 found the streamer loading for the new shot before dropping the
  old one, and the scenery running three passes behind at a 1024 MB pool. Several passes behind is
  what a staircase looks like from the inside.

### What would settle it

A `streaming_trace=1` capture of a fresh load and of a mid lap restart of the same session,
compared on how many passes each takes to settle and what the pool holds when each starts. The
telemetry already records the tile queue and the pool figures per second, so the two runs can be
laid side by side.

### Notes

Reported from the owner's machine, the RTX 3060 Laptop, with the mod on. Not yet checked with the
mod off.

### Eight Nordschleife runs, 2026-10-08

All in `logs/writing-steps-20261008`, at 1024 MB with a join, a lap and a mid lap restart each.

- The first run with every streamer fix on restarted very slowly. A run with them all off, and a run
  with only the rank fix off, restarted faster. The trace of the slow run showed admitted textures
  waiting for room at a load gate of 0, the player's car among them.
- From the exe, the admission budget counts the coarsest level of every tracked texture as free, but
  a texture left out keeps that level and is never charged for it, so about 1,500 tiles are promised
  twice and what arrived last waits for them. A restart moves the car rather than reloading the track,
  and for about 12 s after any move the game keeps asking for the textures of the spot the car left.
- A fix that charged those levels before the admission walk cut the waiting tiles after a restart
  from about 1,600 to 160. With a preference for loaded levels added against the churn it caused, the
  owner still saw it as much blurrier than 0.3.2, and a restart stuck one step short of sharp. The
  exact charge pushes the least important textures to their coarsest copy, which shows more than the
  detail it buys elsewhere.
- The 0.3.2 release and plain 0.4, both with the same over promise and the same waiting numbers as the
  slow run, looked sharp to the owner and restarted near instantly, plain 0.4 twice.

## Fix

Won't fix, on the owner's call of 2026-10-08. The symptom did not come back on the 0.3.2 release or on
plain 0.4, and the owner puts the early slow restarts down to caching. The trace cannot tell that from
the variance between restart spots. The over promise is real, but correcting it looked worse, so the
branch that carried it was deleted. If slow restarts come back, the first step is a by eye A/B of the
release against the current build with two restarts each.

## Verification

Absent.
