---
title: Menu icons stop showing after a while, in the home screen and the car select screen
status: fixed
severity: minor
reported: 2026-09-05
updated: 2026-09-05
source: user, "The icons in home stop working after a while (the main menu in home, for the cars as well in car select screen)"
---

## Symptom

In the stock game, after some time in the menus, the icons in the home screen and the car
thumbnails in car select stopped rendering.

## Evidence

- User report on 2026-09-05 after the mod was installed: "the image thing I told is already
  fixed, happens after a few ms but fixed. main game had that issue."
- Menu images are uploaded through the DirectStorage queue `GpuUpload Memory Queue` with
  destination `TEXTURE_REGION`. In the stock game the two 1024 MB staging buffers left about
  1 GB of the 6 GB budget for everything else, so late UI uploads were the first thing to fail
  or be evicted.

## Suspects

- VRAM starvation from the staging buffers (consistent with the fix).

## Fix

The default `staging_buffer_mb=128` in the mod ini. Verified by the user in normal menu use on
2026-09-05. Icons still take a few milliseconds to appear, which is the upload itself.
