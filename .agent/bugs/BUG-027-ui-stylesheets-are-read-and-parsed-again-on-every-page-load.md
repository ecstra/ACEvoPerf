---
name: BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load
kind: bug
description: the game reads and parses its 1.2 MB of UI stylesheets again at every document load, pause, resume, back to the pits and each main menu page, 38 MB of repeated reads in a 20 minute race session plus a full parse on the render thread each time, while the 2.3 MB script is read once
updated: 2026-09-14
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, ui-lag-deepdive-2026-09-14, TODO-021-the-engine-reads-the-same-data-twice, TODO-027-the-ui-developer-build-and-one-session]
status: open
severity: bug
area: ui
reported: 2026-09-14
parent: BUG-014-ui-pages-lag-on-open-switch-and-interaction
---

## Problem

Every change of UI document costs one frame of 115 to 207 ms and 250 to 670 ms of stall. Part of it is the
stylesheets, which the game fetches and parses again at every load although they never change in a
session. Redundant IO and parsing are in scope whatever the seconds.

## Evidence

From the deep dive of 2026-09-14,
[ui-lag-deepdive-2026-09-14](../docs/research/ui-lag-deepdive-2026-09-14.md).

- `uicomponents.css` (1,153,706 bytes) and `ui.css` (48,807 bytes) are reread at every document load, 33
  and 34 times in the 20 minute `logs/fix1-ai30-20260913` session, 38.2 MB of repeated UI reads, while
  `components.js` is read once per process and the game preloads its `.js` files at startup
  (`0xDEF6D0`).
- The stylesheet's parse error near `-2ren` is logged after every page load, 30 times for 29 loads in
  census-ai30, so the sheet is parsed again each time.
- The parse costs 0 to 45 ms on the render thread per load, inside the stall frame.

## Fix

Absent. The candidate calls the Cohtml system's `PreloadAndCacheStylesheet` (slot 22) for both sheets once
after `Library::CreateSystem` returns. Medium risk, Coherent fixed a reuse failure in 1.65 and a crash on
removing a `<link>` to a preloaded sheet with media rules in 3.1.1, and `uicomponents.css` has two
`@media` blocks. Proven when the parse error appears once per process, killed by an unstyled page or a
crash on the first page switch.

## Verification

Absent.
