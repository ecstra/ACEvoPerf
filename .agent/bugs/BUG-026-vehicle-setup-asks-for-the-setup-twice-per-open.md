---
name: BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open
kind: bug
description: the vehicle setup page sends CarSetupRequestInit twice a few milliseconds apart on every open, and each answer rebuilds all 18 setup groups and 4 info panels with 36 whole page navigation scans, so the page's heavy open build runs twice
updated: 2026-09-14
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, ui-lag-deepdive-2026-09-14, BUG-025-controls-page-scans-the-page-once-per-new-row, TODO-027-the-ui-developer-build-and-one-session]
status: open
severity: bug
area: ui
reported: 2026-09-14
parent: BUG-014-ui-pages-lag-on-open-switch-and-interaction
---

## Problem

Owner wording from BUG-014: "And the vehicle setup in the pre-lap menu also lags." Opening vehicle setup
stalls, and the page's open work is done twice.

## Evidence

From the deep dive of 2026-09-14,
[ui-lag-deepdive-2026-09-14](../docs/research/ui-lag-deepdive-2026-09-14.md).

- `CarSetupRequestInit` appears twice a few milliseconds apart on every visit in five game logs of both
  game versions, among them ui3 at 10:45:18.443 and 10:45:18.445 and ai30-A-fix-on at 13:13:48.999 and
  13:13:49.001.
- Each response rebuilds all 18 `ks-setup-group` and 4 `ks-setup-infopanel` elements, runs 36 whole page
  navigation scans (the same scans as BUG-025) and adds 36 change listeners per slider. In ui3 the open
  frames were 237.8 ms then 80.0 ms, with 697 new elements in the first second. How those two frames split
  between the two builds is not measured.
- A second visit in the same document keeps the first visit's 22 `VehicleSetup.Init` handlers.

## Fix

Absent. The candidate is a guard in `VehicleSetupPage.prototype.init` that ignores a call while its own
request is outstanding, so the page keeps one response. About an hour, delivered with BUG-025's script and
tested by TODO-027's build.

## Verification

Absent.
