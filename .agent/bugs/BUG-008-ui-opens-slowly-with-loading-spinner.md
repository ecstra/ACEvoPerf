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
- Smoke test 2026-09-05 17:38 with the flag on through the mod: the game log gained
  `[ui] [info] Preloaded 1037 files for 181 MB` six seconds after start, no errors, menu VRAM
  unchanged (2999 MB). Every UI page is then already in memory when opened.

- Owner after lap two of 2026-09-05 with the preload on: "the UI is still laggy on open and
  switching. nothing changed." So page resource loading is not the wait.
- Remaining suspects: the UI talks to the game core over a local WebSocket
  (`WebSocketCoreConnection.cpp`, `HttpServer.WindowsDesktop.dll`) with request and response
  protobuf messages, and the spinner is the UI waiting for a response. The car select screen also
  loads whole car assets and thumbnails through the streaming queues (5.6 GB of file to memory
  traffic in the four minute session that ended in the showroom).

## Fix

Absent. `ui_force_resource_preloading` is ruled out. Next: time the gap between the `goTo` line
and the matching `Init: menuState updated` line in the game log for the pages the owner names,
and the request and response pairs in between.

## Verification

Absent.
