---
name: BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three
kind: bug
description: the pit lane menu and every page opened from it, vehicle setup, settings and controls, advance and paint the UI view only one game frame in three, the rotation the engine uses for the HUD while driving, because its every view every frame rule covers only the main menu showroom and pause
updated: 2026-09-14
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, ui-lag-deepdive-2026-09-14, one-percent-lows-2026-09-14, TODO-027-the-ui-developer-build-and-one-session, TODO-025-the-ui-view-rotation-test]
status: open
severity: bug
area: ui
reported: 2026-09-14
parent: BUG-014-ui-pages-lag-on-open-switch-and-interaction
---

## Problem

In the pit lane menu before a lap, and on every page opened from it, the menu ticks 28 to 38 times a
second while idle and about 10 times a second while busy, input reaches it up to two frames late, and
script delays counted in frames take three times as long. The same pages opened from the main menu or
from pause update every frame. It is part of what the owner described as the menu inside the map lagging.

## Evidence

From the deep dive of 2026-09-14,
[ui-lag-deepdive-2026-09-14](../docs/research/ui-lag-deepdive-2026-09-14.md).

- `GameUi::PostFrame` (`0xde3650`) advances every listed view each frame only when `GameUi+0x555` or
  `+0x556` is set, otherwise one view a frame in rotation over the HUD and the car's display views.
  `+0x555` is set only in `PaintShopGameModeClient`, the main menu showroom, and `+0x556` only in pause or
  a stopped replay, both through EvoUi slot 24 from `GameScenePresenter::update` at `0x96b5cf`.
- Across 513 windows in 60 sessions, heavy frames on pit lane settings and vehicle setup fall every third
  frame in 100 percent of gaps (179 heavy frames on 0.9.0, 31 on the 0.9.1 pit lane main page), against
  14 to 18 percent in the main menu and pause.
- The hunt's "vehicle setup at 12 to 29 fps" was the page's own tick rate on that rotated view, while the
  frames presented at a median of about 82 fps.

## Fix

Absent. The candidate is a wrapper on EvoUi slot 24 that passes the menu flag while the main view's
document is not `hud.html`, the path pause already takes, or a budget variant that keeps the main view in
every frame and rotates only the displays. On heavy pages it trades a smoother scene for a menu that
updates more often, so the owner judges the result. Tested by TODO-027's build.

## Verification

Absent.
