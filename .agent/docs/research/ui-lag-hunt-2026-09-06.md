---
name: ui-lag-hunt-2026-09-06
kind: doc
description: one morning of instrumented runs into the menu and settings page lag, every lever the DLL has was tried and measured, where the cost really sits and what a fix would take
updated: 2026-09-06
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, cohtml-ui-engine, DEC-010-no-ui-changes-ship, TODO-011-ui-overhaul-through-injected-scripts, telemetry]
---

# The UI lag hunt, 2026-09-06

The owner's complaint (BUG-014): the three menus lag a little, and the settings, controls and
vehicle setup pages inside them lag a lot, on open, on every switch and while they are used.
This is the record of the morning, so the next attempt starts from the numbers. Session
folders `logs/ui1-inspector` to `logs/ui16-relayout` hold the raw files.

## Instruments

- The Cohtml hook in `src/ui/cohtml.cpp` with the inspector switch (`cohtml-ui-engine`).
- A per second account of `Library::ExecuteWork` by thread and work type, wall against CPU
  time, with the exe call sites, plus a layout thread of the mod and a V8 flags switch. All in
  the history before the commit `removed: the UI lag hunt instruments and script patches`.
- A JavaScript probe pushed through the inspector (`Runtime.evaluate`) that wrapped
  `requestAnimationFrame`, the engine's event handlers, `engine.call`, the model updates and
  the forced layout reads, reporting counts and milliseconds per second, and a command channel
  to flip switches in the live page. Scratchpad scripts of the session, not in the repo.
- `ui_transitions.py` (scratchpad): the frames CSV lined up with the game log's page markers,
  the cost of every page switch as the worst frame and the total stall in the two seconds after.

## What was measured

Baseline (race paused at 85 fps, main menu locked at 60):

| transition | worst frame | stall in 2 s |
| --- | --- | --- |
| pause, settings open, back to the HUD (each a document reload) | 120 to 160 ms | 160 to 460 ms |
| controls page open | 135 to 155 ms | 800 to 1000 ms |
| bindings group switch | 260 to 360 ms | 1000 to 1500 ms |
| vehicle setup open | a few seconds at 12 to 29 fps | |

Idle on any page the frame time is the game's own (3 to 4 ms of layout a second on the
controls page, 85 fps in a paused race). While the owner hovers or scrolls the controls page:
10 to 40 fps, 200 to 800 ms of layout a second on the render thread, 5,000 to 16,000 forced
layout reads a second, 250 to 600 new elements a second, 300 to 800 ms of script a second in
frame callbacks.

Levers tried, each in its own build and run:

1. V8 flags `--no-flush-bytecode --sparkplug` before the script engine starts: nothing.
2. The render thread was executing Cohtml's layout tasks itself (wall equals CPU time). A
   thread of the mod taking the type 1 work notifications and running them: the render thread's
   share went to zero, the transitions stayed identical, the render thread waits for the layout
   result anyway.
3. Pausing the per frame model refresh (`ksUI.pauseModelSync`): no change in layout time.
4. One visibility loop per element instead of six to eleven (the loops leak on every restore):
   the loop callbacks went from 121,000 to 10,000 a second, layout time unchanged.
5. One visibility sweep every 125 ms, all reads first, then all body moves: the forced reads
   fell from 11,000 to about 1,000 a second, layout time and the transitions unchanged.
6. The V8 sampling profiler through the inspector: crashed the game after three seconds.

Two side findings: the UI view stops advancing while the game window is not active and resumes
on real input, which reads as a frozen menu after a window switch, and one experiment interval
left running across a page change threw an error every frame and jammed the UI, so scripts
driven from outside must stop themselves when their target is gone.

## Where the cost sits

Three costs, none reachable from the engine side:

- The document reload on every page switch, 120 to 160 ms in one frame: the 2.3 MB bundle, the
  1.1 MB stylesheet and every component initialised again. Only keeping one document alive
  removes it (the shell attempt of 2026-09-05, TODO-008 in the history, swapped pages in 4 to
  11 ms but the pages' own build stayed).
- The synchronous list builds: 100 rows of the controls page created, styled and laid out in
  one call, 260 to 360 ms per group switch.
- Relayouts during interaction: a layout pass over a 1,200 element page against 5,300 rules
  costs tens of milliseconds in Cohtml, and the scripts trigger one on nearly every frame of a
  hover or a scroll (focus box moves, scrollbar thumb writes, row bodies restored and
  initialised, focus and blur storms).

The engine levers (work placement, threads, script flags, inspector) do not change any of the
three. The script side patches that were tried are correct fixes of real defects (loop leak,
read and write interleaving) but they do not touch the dominant costs, so the owner felt no
difference.

## What a fix would take

An overhaul of the pages' scripts, delivered as an initial script of the view so no UI file is
replaced (TODO-011): rows built in slices across frames, restores and focus moves that do not
touch layout properties, the model refresh only when a model changed, one document kept alive
for the in race pages. Days of work against a 50,000 line bundle, with the risk shown on
2026-09-05, and no engine shortcut. Not started, the owner's call (DEC-010).
