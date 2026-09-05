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

- Session of 2026-09-05 17:47 to 18:03 (three race loads, two car changes, pause menu opened
  three times, settings opened once): every page switch is a full document reload of the UI
  (`Loading page X` then `Gameface Engine Ready`, `Init Components`, model registration, `Init:
  menuState updated`). Measured from the game log: pause menu 0.18 to 0.30 s from `Loading page`
  to `PauseMenu Show`, settings 0.24 s, main to single player 0.13 s, return to pit lane 0.20 to
  0.29 s. Every switch is logged with `transition: true`, the spinner overlay is part of that
  transition. Leaving a race to the menu is 3.3 s because the menu scene and the car are reloaded
  (`Track resources streaming took 1.76 s`, car graphics 1.44 s).

- The mod's frame log at the same timestamps shows the render thread stalling during each
  reload: pause menu open 61 ms plus 127 ms (twice more: 61 plus 45 plus 142 ms, and 63 plus 35
  plus 150 ms), settings page open 36 plus 102 ms and then a 1.2 s stretch at 23 fps with a
  322 ms frame while the page builds its controls, back to the pit lane 161 plus 51 ms. Streaming
  is idle in those frames (`tiles 0 req` on the hitch lines), the stall is UI script and layout
  work on the render thread.

## Fix

Absent. Root cause is the UI framework reloading the whole document and re running its
initialisation on every page switch, on the render thread. `ui_force_resource_preloading` is
ruled out. A fix from outside the game means changing the UI itself (the HTML and JS under
`uiresources\` in the package, which would need a repack), so this is deferred behind the render
work.

## Verification

Absent.
