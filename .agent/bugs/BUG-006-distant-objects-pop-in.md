---
name: BUG-006-distant-objects-pop-in
kind: bug
description: distant objects switch detail level visibly as the car approaches
updated: 2026-09-05
links: [settings-files, TODO-005-lap-two-experiments]
status: open
severity: nit
area: render
reported: 2026-09-05
parent:
---

## Problem

Owner wording: "Farther items pop-in and does not folow the same level we have?"

## Evidence

- Level of detail is the `graphics.levelOfDetail` block of `video.videosettings`. The custom mode
  exposes `mainLodDistanceScale`, `mirrorLodDistanceScale`, `cubemapLodDistanceScale`,
  `shadowLodDistanceScale` and the matching `...OutDistanceScale` values (schema in
  `tools/data/proto_schema.txt`).
- The owner's file on 2026-09-05 16:25 has the High preset with every scale at 1.0.

## Fix

Absent. Raising `mainLodDistanceScale` costs GPU time on a GPU that is already pinned (BUG-002),
so it is queued behind the frame rate work in TODO-005.

## Verification

Absent.
