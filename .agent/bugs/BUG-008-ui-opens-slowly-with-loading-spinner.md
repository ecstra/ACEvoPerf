---
name: BUG-008-ui-opens-slowly-with-loading-spinner
kind: bug
description: opening any UI page in race or in the menu lags and shows the loading spinner
updated: 2026-09-05
links: [engine-flags, TODO-005-lap-two-experiments, directstorage-streaming]
status: open
severity: bug
area: ui
reported: 2026-09-05
parent:
---

## Problem

Owner wording: "Opening UI (any in-race, or in menu) lags and takes a while to load (showing the
ace loading). The lag and loading needs to go. Its a UI for christs sake."

## Evidence

- The UI is Coherent Gameface. Every page is an HTML document under `uiresources\` in the
  content package (949 files, XOR ciphered), loaded on demand: the game log shows
  `goTo menu.html coui://uiresources/menu.html ... transition: true` style lines each time a page
  opens, and `Detected deprecated behavior - relying on SynchronizeModels being queued to run after
  DOMContentLoaded` warnings from the UI framework on the same transitions.
- Page loads go through the `FileToMemory Queue` (package to CPU memory, XOR decode on the CPU)
  and then V8 parse and Cohtml layout, all on demand.
- The engine flag `ui_force_resource_preloading` ("force resource preloading also in dev builds",
  `GameUi.cpp`) is false in the release build. Its name says the shipped build only preloads in
  dev builds.

## Fix

Absent. First experiment: `ui_force_resource_preloading=true` through the mod ini, measured by
the time between the `goTo` line and the page's first render in the game log, and by feel.

## Verification

Absent.
