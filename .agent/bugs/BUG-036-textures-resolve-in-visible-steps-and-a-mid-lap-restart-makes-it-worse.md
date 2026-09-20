---
name: BUG-036-textures-resolve-in-visible-steps-and-a-mid-lap-restart-makes-it-worse
kind: bug
description: textures come up mushy and then sharpen in visible stages rather than in one go, and a session restart mid lap makes the staircase worse or slower, which points at the mip chain arriving level by level and at a restart starting from a worse position than a fresh load
updated: 2026-09-20
links: [directstorage-streaming, BUG-018-whole-scene-low-detail-for-a-second-after-load, BUG-010-texture-pool-shrinks-on-race-load-and-restart, BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring, BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles, texture-streamer-camera-cuts-2026-09-14]
area: streaming
status: open
severity: bug
reported: 2026-09-20
parent:
---

## Symptom

Owner, 2026-09-20: "The textures are mushy and blurry at the start, then it becomes less blurrier
and then less blurrier before finally loading. (worsens or slows on session restart mid lap)."

Two parts, and the second is the interesting one.

The staircase: the detail does not arrive in one step. The scene starts mushy, sharpens part of
the way, sharpens again, and only then settles. That is the mip chain being handed over level by
level with each level held long enough to see.

The restart: doing a session restart from mid lap makes it worse or slower than the same scene
loaded fresh. A restart reuses a process that has already been driving, so whatever state it
starts from is not the state a fresh load starts from.

## Why it is filed separately from BUG-018

BUG-018 is the whole scene at low detail for one or two seconds after the curtain lifts, at game
start and at track entry, settling on its own. This is a different shape in both halves. The
staircase is several visible levels rather than one coarse moment, and the trigger that makes it
worse is a mid lap restart, which BUG-018 does not cover at all.

They may turn out to be the same mechanism seen from two angles, in which case one of them folds
into the other. Filing it separately keeps the restart evidence from being lost inside a record
that does not mention restarts.

## What it probably touches

- BUG-010 measured the engine resizing the texture tile pool during scene transitions, 633 MB in a
  race and 526 MB after a restart, and was fixed by the fixed pool size. A restart being worse is
  the same trigger, so whether anything of that survives the fix is the first thing to check.
- The camera cut work of BUG-021 found the streamer loading for the new shot before dropping the
  old one, and the scenery running three passes behind at a 1024 MB pool. Several passes behind is
  what a staircase looks like from the inside.

## What would settle it

A `streaming_trace=1` capture of a fresh load and of a mid lap restart of the same session,
compared on how many passes each takes to settle and what the pool holds when each starts. The
telemetry already records the tile queue and the pool figures per second, so the two runs can be
laid side by side.

## Notes

Reported from the owner's machine, the RTX 3060 Laptop, with the mod on. Not yet checked with the
mod off.
