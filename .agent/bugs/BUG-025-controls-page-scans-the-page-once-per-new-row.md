---
name: BUG-025-controls-page-scans-the-page-once-per-new-row
kind: bug
description: clicking a bindings group on the controls page freezes one frame for 250 to 500 ms because every new row makes the navigation library scan the whole page once per navigation section, 3.1 to 3.4 ms a row, while the rows are still parked outside the document so the scans change nothing
updated: 2026-09-14
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, ui-lag-deepdive-2026-09-14, TODO-027-the-ui-developer-build-and-one-session, TODO-011-ui-overhaul-through-injected-scripts]
status: open
severity: bug
area: ui
reported: 2026-09-14
parent: BUG-014-ui-pages-lag-on-open-switch-and-interaction
---

## Problem

Owner wording from BUG-014: "controls page, it takes nearly a second to load (it loads, just very very
slowly and lagging the entire game and keeps the game at lagged fps while editing)". Switching a bindings
group freezes the game for one long frame, and opening the page carries a shorter one.

## Evidence

From the deep dive of 2026-09-14,
[ui-lag-deepdive-2026-09-14](../docs/research/ui-lag-deepdive-2026-09-14.md), over 60 switches in the
hunt's sessions ui3, ui4, ui7 and ui13.

- The freeze is the third frame after the group's rows are built and grows in a straight line with the
  rows, 3.12 ms a row over 23 clean switches. Its clean medians are 50.6 ms for Showroom's 12 rows,
  85.0 ms for UI's 29, 260.9 ms for Camera's 74, 274.0 ms for Car's 81 and 336.6 ms for Car_Advanced's
  105. The build frame itself is 17 to 52 ms.
- Every row is lazily loaded with its body in a DocumentFragment (`components.js` lines 1014 to 1016).
  Its `setupNavigation` (line 7941) runs one and three frames after connect and calls `refocusNav` (line
  7870), which calls `SpatialNavigation.makeFocusable()` with no section, and that queries `.focusable`
  once for each of the four sections (line 3274). No row is restored into the document by that frame, so
  every call scans the same elements and adds no `tabindex`.
- The freeze frame carries no Cohtml layout work (a regression of minus 0.02 against 0.62 for other
  frames over 62 windows), so it is script.
- At a page open the same step lands one frame later at 95 to 139 ms, and from the pit lane it costs
  290 ms because the page runs one UI frame in three (BUG-024).

## Fix

Absent. The candidate patches `setupNavigation` to return early for a lazily loaded row still in its
fragment, and folds the other calls into one stock `makeFocusable()` on the next animation frame, with the
stock method as fallback, delivered as a script ahead of `components.js` through the package override
layer or an initial script. About 30 lines. The overhaul round of 2026-09-06 broke the controls list by
reading the filter input before it existed, so the patch touches nothing that runs before the page is
built. Tested by TODO-027's build.

## Verification

Absent.
