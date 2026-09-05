---
title: Game sometimes crashes while starting
status: open
severity: crash
reported: 2026-09-05
updated: 2026-09-05
source: user, "crashes while opening"
---

## Symptom

Some launches end before the menu appears.

## Evidence

- None collected yet. Every launch done by the agent on 2026-09-05 (about twelve, with and without
  the mod) reached the menu.

## Suspects

- Same causes as [crash-on-car-or-track-change.md](crash-on-car-or-track-change.md), since the
  startup also loads a full scene.
- Something specific to the first scene load after a cold boot (shader cache, driver). Supported if
  the crashes cluster on the first launch of the day.

## Fix

Not yet.
