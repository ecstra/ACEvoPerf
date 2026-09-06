---
name: TODO-011-ui-overhaul-through-injected-scripts
kind: todo
description: the overhaul of the UI's page scripts that would remove the felt lag, one round tried and dropped on the owner's word, everything UI related removed from the mod
updated: 2026-09-06
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, ui-lag-hunt-2026-09-06, DEC-010-no-ui-changes-ship]
status: dropped
by: agent
area: ui
born: 2026-09-06
done: 2026-09-06
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

Dropped on 2026-09-06. Items 1, 2 and 4 went into one build on the owner's word (described in
`ui-lag-hunt-2026-09-06`, the code was not kept), the owner drove it, felt no change and found
the controls list empty (the row build read the filter input before it existed), and closed
the topic: "Remove everything related to UI." The success check, had it continued: the
group switch and the page opens show no frame over 100 ms in `acevo_perf_frames.csv`, hovering
and scrolling the controls page holds the page's idle frame rate, the pause and resume cost no
document reload, and every page still looks and behaves as before across the menus, a race and
a replay.
