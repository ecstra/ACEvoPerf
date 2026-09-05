---
name: BUG-003-menu-icons-stop-rendering
kind: bug
description: home and car select icons stopped rendering after a while in the stock game
updated: 2026-09-05
links: [DEC-003-staging-buffer-128mb, directstorage-streaming]
status: fixed
severity: bug
area: ui
reported: 2026-09-05
parent:
---

## Problem

Owner wording: "The icons in home stop working after a while (the main menu in home, for the cars
as well in car select screen)". Seen in the stock game.

## Evidence

- Menu images go through the DirectStorage queue `GpuUpload Memory Queue` with destination
  `TEXTURE_REGION` (proxy log).
- In the stock game the two 1024 MB staging buffers left about 1 GB of a 6 GB budget for
  everything else, so late UI uploads were the first casualty.
- Owner on 2026-09-05 after the mod: "the image thing I told is already fixed, happens after a
  few ms but fixed. main game had that issue."

## Fix

Root cause: video memory starvation by the staging buffers. Fixed by the default
`staging_buffer_mb=128` in `dist/acevo_perf.ini`, commit 0140743, 2026-09-05.

## Verification

Owner report in normal menu use on 2026-09-05. Icons still take a few milliseconds to appear,
which is the upload itself.
