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

- How the pages hang together (read from `uiresources\js\components.js`, 2.3 MB, one bundle
  shared by all 19 pages, plus `css\uicomponents.css` 1.1 MB): `ksUI.goTo(page, path)` calls
  `window.location.replace(page)` whenever the target page is not the current document, so the
  HUD (`hud.html`) to pause menu (`ingame.html`, path `pause`) switch is a document swap, resume
  (`GameMode Resume`) swaps back to `hud.html`, settings is a third document. Only a path change
  inside the same document (`changePage`) is cheap. Every document re parses the bundle and the
  CSS, re registers 19 data models and re binds.
- Page timing markers through the override layer (`acevo_pagetiming.js`, 19:49 session, 26 page
  loads). Time from the first script of the new document to `DOMContentLoaded` (the bundle
  compiled and run) and to `load`:

  | document | elements | to DOM | to load |
  | --- | --- | --- | --- |
  | `menu.html` | 548 | 80 to 137 ms | 96 to 178 ms |
  | `settings.html` | 747 to 1530 | 100 to 118 ms | 140 to 163 ms |
  | `ingame.html` (pause menu) | 989 | 99 to 136 ms | 147 to 193 ms |
  | `hud.html` | 40 | 152 to 235 ms | 157 to 240 ms |

  The HUD document has 40 elements and still costs 150 to 235 ms, so the cost is the bundle, not
  the page content. `PauseMenu Show` comes about 330 ms after `Loading page`. One pause and
  resume is two documents, 300 to 500 ms of render thread stalls. Switching away from the game
  and back while paused does the same on the return (the resume reloads `hud.html`), which is
  why the owner's 1 percent low counter drops after every window switch (19:55:14 return: worst
  frame 131 ms, 18 frames over 20 ms in the next 10 s). Clean driving before the switch measured
  98 fps with a 1 percent low of 77 fps and p99 12.3 ms, so the pacing itself is intact.

## Fix

Absent. Root cause is the UI framework reloading the whole document and re running its
initialisation on every page switch, on the render thread. `ui_force_resource_preloading` is
ruled out. The override layer (TODO-007, done) lets loose files replace `uiresources\*`.
Candidates, cheapest first:

1. Keep the HUD and the pause menu in one document: serve a `hud.html` that also holds
   `ks-ingame` and `ks-pausemenu`, and patch `goTo` in the bundle so `hud.html` and
   `ingame.html` count as the same document (path change instead of `location.replace`). Removes
   the two reloads per pause. Needs the body class and `data-bind-if` on the menu to follow
   `ModelMenuState.activeMenu`.
2. Cut the per document cost: drop `ks-dev-reloadbutton`, minify the bundle and the CSS, defer
   components a page never uses. Smaller gain, no behaviour change.
3. The settings page stays its own document, its 1.2 s of control building is layout work, to be
   measured with the markers before touching it.

## Verification

Absent.
