---
title: Distant objects pop in instead of appearing at the same detail level as nearby ones
status: open
severity: minor
reported: 2026-09-05
updated: 2026-09-05
source: user, "Farther items pop-in and does not follow the same level we have?"
---

## Symptom

Objects further away switch detail level visibly as the car approaches.

## Evidence

- Level of detail is a video setting (`graphics.levelOfDetail`), with a custom mode that exposes
  `mainLodDistanceScale`, `mirrorLodDistanceScale`, `cubemapLodDistanceScale`,
  `shadowLodDistanceScale` and the matching `...OutDistanceScale` values. The user's settings file
  read on 2026-09-05 12:42 had `LevelOfDetailQuality_High` with all scales at 1.0.
- `experimentalStaticLevelOfDetail` and `staticLevelOfDetail` exist as well and are off by default.

## Suspects

- Distance scales at 1.0 with the High preset are simply short for a 20 km track. Test: set the
  custom mode with `mainLodDistanceScale` 1.5 to 2.0 through `tools/acevo_settings.py` and check
  the frame cost in the timeline.

## Fix

Not yet.
