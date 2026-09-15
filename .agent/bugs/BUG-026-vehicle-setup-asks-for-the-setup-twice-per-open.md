---
name: BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open
kind: bug
description: the vehicle setup page sends CarSetupRequestInit twice a few milliseconds apart on every open, and each answer rebuilds all 18 setup groups and 4 info panels with 36 whole page navigation scans, so the page's heavy open build runs twice
updated: 2026-09-15
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, ui-lag-deepdive-2026-09-14, BUG-025-controls-page-scans-the-page-once-per-new-row, TODO-027-the-ui-developer-build-and-one-session, responsive-ui, responsive-ui-rounds-2026-09-15]
status: fixed
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

A page fix of the responsive UI, in the same script as BUG-025's (see
[responsive-ui](../docs/systems/responsive-ui.md)). When `ks-page-vehiclesetup` is defined its prototype's
`init` is wrapped: a second call on the same element while its own `Init` request is out is ignored, the
mark clearing when the answer arrives or after three seconds. Calls on other elements always run. First
built into the probe (`5ef8d7d`, corrected in `b4514a3` because the page opens inside the pit menu's own
document), shipped in `6848b45` on 2026-09-15.

## Verification

The owner on 2026-09-14, once the hover lag was fixed: "The sliders in vehicle setup: FIXED". The lap of
2026-09-15 counted 2 inits with 1 ignored.
