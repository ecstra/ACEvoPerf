---
name: BUG-005-crash-on-startup
kind: bug
description: some launches ended before the menu appeared, fixed by the staging buffer cap
updated: 2026-09-05
links: [BUG-004-crash-on-car-or-track-change, TODO-003-capture-crash-evidence, DEC-003-staging-buffer-128mb]
status: fixed
severity: breaks
area: stability
reported: 2026-09-05
parent:
---

## Problem

Owner wording: "crashes while opening".

## Evidence

- None yet. Every launch done by the agent on 2026-09-05, about fifteen with and without the
  mod, reached the menu.
- Same evidence sources as BUG-004 apply.

## Fix

Same root cause as BUG-004, video memory exhaustion while the first scene loads next to the two
1024 MB staging buffers. Fixed by the default `staging_buffer_mb=128`, commit 0140743, 2026-09-05.

## Verification

Owner on 2026-09-05: "the VRAM fix indeed fixed the car switching crashed the game and startup
crash atleast on my machine". About twenty launches by the agent the same day all reached the
menu.

