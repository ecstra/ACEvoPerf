---
name: BUG-030-menus-drop-to-50-to-60-fps-while-moving-things-quickly
kind: bug
description: with the responsive UI on, moving something quickly in the static menus still drops the frame rate to 50 to 60 fps and the 1 percent low at times to 22 fps, and the UI probe caught the vehicle setup page restyling its whole page nine times a second through an attribute change on one of its components while the mouse moved
updated: 2026-09-16
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, BUG-028-page-opens-still-hold-frames-of-100-to-200-ms, BUG-029-the-hud-restyles-most-of-its-page-while-driving, responsive-ui, telemetry]
status: open
severity: nit
area: ui
reported: 2026-09-15
parent: BUG-014-ui-pages-lag-on-open-switch-and-interaction
---

## Problem

In the owner's words, after the child removal fix: "We made the UI responsive but it drops the FPS to 50-60 when
rapidly moving something (sometimes the 1% is at 22fps) but thats lower class issue since its a static UI not
while driving." Which page and which movement (sweeping the mouse over rows, dragging a slider) is not named yet.

On 2026-09-16 the owner listed it with the remaining memory leak as what is left, "the fps and 1% reducing when
opening and interacting with the UI, the responsive UI". Page opens holding frames are BUG-028.

## Evidence

`logs/children-fix-20260915`, the pit lane menu's vehicle setup page with the UI probe on, 22:19:24 to 22:19:27,
while the mouse moved over the page (up to 114 mouse moves and 92 hover changes a second in those seconds, and the
focus box's `top`, `left`, `bottom` and `right` written 31 to 38 times a second):

| Second | fps | Longest frame |
| --- | --- | --- |
| 22:19:24 | 98.9 | 40.1 ms |
| 22:19:25 | 95.9 | 41.0 ms |
| 22:19:26 | 75.9 | 46.7 ms |
| 22:19:27 | 75.0 | 53.7 ms |

- **The whole page restyles six times a second.** Each of those seconds has six restyle passes of 1,161 nodes from
  1,161 changed, 26.8 to 33.4 ms each.
- **An attribute change on one component marks it all.** Each second has 9 attribute invalidations (kind 7) on one
  custom element with the class `delayedInit`, 9,864 marks in all and up to 1,160 in one.
- **It predates the child removal fix.** The same attribute invalidations of more than a thousand marks show in
  `logs/ui-fix-e-20260914` (8 seconds) and `logs/ui-fix-f-20260915` (5 seconds).

The page script counted 6 to 15 `setAttribute` calls a second there, it does not see `dataset` writes, and the
probe names the element of an invalidation but not the attribute, so what is written is not known.

## Fix

Absent. First the probe names the attribute and the call stack of every attribute invalidation that marks a few
hundred nodes, the way it named the child list change of BUG-029, on the page and movement the owner names. Then
the same question as BUG-029: whether the marks are Cohtml's invalidation being coarser than the stylesheets need.

## Verification

Absent.
