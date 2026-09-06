---
name: TODO-011-ui-overhaul-through-injected-scripts
kind: todo
description: the overhaul of the UI's page scripts that would remove the felt lag, delivered as a Cohtml initial script from the DLL, not started, the owner's decision
updated: 2026-09-06
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, ui-lag-hunt-2026-09-06, cohtml-ui-engine, DEC-010-no-ui-changes-ship]
status: open
by: agent
area: ui
born: 2026-09-06
done:
---

## What

The work the lag hunt found necessary for a felt improvement, in the order of the measured
cost, each a script patch of the game's own functions delivered through `View::AddInitialScript`
(slot 61, runs at the start of every document) so no UI file is replaced:

1. Build the controls page rows in slices across frames (`populateBindings`, about 100 rows,
   260 to 360 ms in one frame today) and keep built groups instead of rebuilding on every
   switch.
2. Move the focus indicator box and the scrollbar thumb with transforms instead of top, left,
   height and width, and write them only when the value changed (`indicateFocus`,
   `perFrameIndicatorUpdate`, `updateScrollBar`).
3. Refresh a data model only when its content changed (`fetchModel`, `assignModel`), instead
   of five whole model updates per frame.
4. Restore lazily loaded rows in one batch per sweep with their initialisation spread over
   frames (the sweep of the removed patch is the base).
5. Keep one document alive for the in race pages (HUD, pause, pit lane, settings), the shell of
   2026-09-05 (TODO-008 in the history) rebuilt on the initial script vehicle, to remove the
   120 to 160 ms reload per switch.

## Why

BUG-014. Every engine lever was measured and none moves the numbers (`ui-lag-hunt-2026-09-06`).

## Done when

The owner decides to start it (DEC-010). Then: the group switch and the page opens show no frame
over 100 ms in `acevo_perf_frames.csv`, hovering and scrolling the controls page holds the
page's idle frame rate, the pause and resume cost no document reload, and every page still
looks and behaves as before across the menus, a race and a replay.
