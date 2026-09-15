---
name: BUG-028-page-opens-still-hold-frames-of-100-to-200-ms
kind: bug
description: with the responsive UI on, opening the settings, controls or pit lane page still holds one or two frames of 100 to 200 ms, lighter pages 75 to 100 ms, from the page's own script in the frame it appears, pages built over several frames restyling much of the page each frame, and the listed rules still walked per element
updated: 2026-09-15
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, responsive-ui-rounds-2026-09-15, responsive-ui, BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load]
status: open
severity: nit
area: ui
reported: 2026-09-15
parent: BUG-014-ui-pages-lag-on-open-switch-and-interaction
---

## Problem

What is left of BUG-014 after the responsive UI. The owner, after the build that shortened page opens:
"works enough i guess". Opening a page, the settings page, the controls page or the pit lane menu, still
shows a short hitch as the page comes in.

## Evidence

From lap G of [responsive-ui-rounds-2026-09-15](../docs/research/responsive-ui-rounds-2026-09-15.md),
`logs/ui-responsive-g-20260915`.

- Settings and controls from the main menu, 328 ms over 16.7 ms in the 1.5 s after the load and frames of
  107.5 and 125.7 ms. The first carries 67.9 ms of Advance, the new document's module and component set up.
- Settings, video from the pit lane, one 201.5 ms frame and 40,627 nodes restyled in that second while 52
  child list changes on the scrolling container each marked its subtree, 30,432 marks.
- The listed rule loop, now skipping rules that cannot match, still visits all 2,142 rules per element, 7 to
  14 percent of layout samples in the stub.
- The stylesheets are parsed at every load, BUG-027.

## Fix

Absent. Candidates, none built:

- file the listed rules by tag and id so the loop visits only its element's, kept current through the rule
  set's add and remove (`0x3E9C40`, `0x3F0100`)
- read how Cohtml's child list invalidation (kind 0) picks what it marks and mark only what the game's
  `:first-child`, `:last-child` and `:nth-child` rules can change
- the page scripts' set up is the game's code and nothing reached it yet

## Verification

Absent.
