---
name: BUG-014-ui-pages-lag-on-open-switch-and-interaction
kind: bug
description: the menu, the in session menu and the pause menu lag, the settings, controls and vehicle setup pages stall on open, on every switch and while they are used
updated: 2026-09-06
links: [BUG-013-one-percent-lows-drop-after-window-or-input-switch, cohtml-ui-engine, ui-lag-hunt-2026-09-06, DEC-010-no-ui-changes-ship, TODO-011-ui-overhaul-through-injected-scripts]
status: open
severity: bug
area: ui
reported: 2026-09-06
parent:
---

## Problem

Owner wording: "So there is three UI surfaces. 1. The menu itself pre-map 2. The menu inside the
map, but before starting the lap 3. the pause menu during the lap. These are a bit laggy. And
even worse, the items within these menus. So settings page within any of these menus lags a lot
when opened. Same for controls page, it takes nearly a second to load (it loads, just very very
slowly and lagging the entire game and keeps the game at lagged fps while editing). And the
vehicle setup in the pre-lap menu also lags." Later: "The issue is not after opening tho, its
during open and close and switching and interaction", "animations is like 10x slow and changing
the pages makes the game stutter and lag and interacting with anything makes it slow".

## Evidence

- Baseline of 2026-09-06 10:41 (race paused, 85 fps steady): every page switch is a full
  document reload costing one frame of 120 to 160 ms (pause, settings, back to the HUD). Opening
  the controls page stalls 800 to 1000 ms in the two seconds that follow. Switching a bindings
  group is the worst single frame, 260 to 360 ms, with 1.0 to 1.5 s of stall per switch. The
  vehicle setup page opens at 12 to 29 fps for a few seconds. Idle on any page the frame time is
  the game's own.
- The JavaScript of those pages is cheap in itself: the probe counted 10,000 callbacks a second
  on the controls page adding up to under a millisecond of script.
- Cohtml creates no threads. Its layout and style work (work type 1) is handed to the game
  through the `OnWorkAvailable` callback, the game posts a job per notification, and its render
  thread executes queued jobs while it waits, so the render thread ran the UI's layout itself:
  180 to 460 ms of every second on the heavy pages, single tasks up to 53 ms, CPU time equal to
  wall time. Moving that work to a thread of the mod changed nothing, the render thread waits for
  the layout result anyway.
- V8 flags (`--no-flush-bytecode --sparkplug`) changed nothing measurable.
- Idle, the controls page costs under 4 ms of layout a second. While the owner hovers or
  scrolls it costs 200 to 550 ms of layout a second, 5,000 to 11,000 forced layout reads a
  second (`getBoundingClientRect` and `getComputedStyle`, 1,400 idle), 250 to 600 new elements a
  second and 300 to 800 ms of script a second in anonymous frame callbacks, at 10 to 40 fps.
- The reads come from the per row visibility loops of the lazily loaded components. The game
  starts a loop on every connect and every restore of a row's body and ends none of them, so a
  scrolled list runs six to eleven loops per row (up to 121,000 loop callbacks a second), each
  forcing a layout read between the writes of the others.
- Two script corrections injected as a Cohtml initial script from the DLL, one loop per element
  and one visibility sweep per 125 ms with all reads before all writes, cut the loop callbacks
  to 10,000 and the forced reads to about 1,000 a second. Layout time, the transitions and what
  the owner felt did not change: a relayout of the 1,200 element page costs tens of
  milliseconds by itself and the scripts trigger one on nearly every frame of a hover or a
  scroll (focus box moves, scrollbar thumb writes, rows restored and initialised).
- A menu that stops responding after a window switch is the game pausing the UI view while its
  window is not active, the view resumes on the next real input. Not a stall.
- The full record with every number is `ui-lag-hunt-2026-09-06`, the engine's surface is
  `cohtml-ui-engine`.

## Fix

Absent. The cost sits in the game's UI pages and scripts, where no engine lever of the DLL
reaches (DEC-010). What a felt fix takes is written up as TODO-011, an overhaul of the page
scripts delivered as an initial script of the view, not started, the owner's decision.

## Verification

Absent.
