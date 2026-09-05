---
name: BUG-011-menu-icons-load-slowly-every-time
kind: bug
description: menu and car select icons take longer to appear and reload on every visit
updated: 2026-09-05
links: [BUG-003-menu-icons-stop-rendering, BUG-008-ui-opens-slowly-with-loading-spinner, package-override-layer]
status: open
severity: bug
area: ui
reported: 2026-09-05
parent:
---

## Problem

Owner wording after the 2026-09-05 19:27 session: "the icons are now taking longer to load (and it
loads every time) (maybe it was happening before and i never noticed it. file it for now)".

## Evidence

- Every UI page is its own HTML document and every page switch reloads it (BUG-008), so images
  referenced by a page are requested again on every visit. The UI's image folder in the package
  is 879 MB (`uiresources\images`), the brand and car artwork 163 MB (`uiresources\branding`),
  each requested through the `FileToMemory Queue` and decoded before display.
- The 19:27 session was the first with the package override layer active (one loose file, the
  main menu page) and with the source split. The layer adds one range check per DirectStorage
  request and does not touch image entries, so a slower icon load is not explained by it yet.
- Not measured. The page timing markers prepared for BUG-008 (`acevo_pagetiming.js`) log the
  page load and the element count, image timing needs its own marker.

## Fix

Absent. Measure first: time from page start to the last icon image load on the main menu, with
and without the override layer, then with the icons served from loose files.

## Verification

Absent.
