---
name: ui-lag-hunt-2026-09-06
kind: doc
description: one morning of instrumented runs into the menu and settings page lag, every lever the DLL has was tried and measured, where the cost really sits, the engine's surface, and why the mod carries nothing UI related
updated: 2026-09-06
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, DEC-010-no-ui-changes-ship, TODO-011-ui-overhaul-through-injected-scripts, telemetry, moddability]
---

# The UI lag hunt, 2026-09-06

The owner's complaint (BUG-014): the three menus lag a little, and the settings, controls and
vehicle setup pages inside them lag a lot, on open, on every switch and while they are used.
This is the record of the morning, so the next attempt starts from the numbers. Session
folders `logs/ui1-inspector` to `logs/ui16-relayout` hold the raw files.

## Instruments

Everything below was removed from the mod on the owner's word at the end of the day. The code
sits in the history between the commits `added: Cohtml engine hooks` and `removed: every UI
related piece of the mod`.

- A hook on the exe's import of `cohtml::Library::Initialize`, following the library to the
  system and the views, with a switch that set the engine's DevTools inspector port.
- A per second account of `Library::ExecuteWork` by thread and work type, wall against CPU
  time, with the exe call sites, plus a layout thread of the mod and a V8 flags switch.
- `ui_probe.py`: a JavaScript probe pushed through the inspector (`Runtime.evaluate`) that
  wrapped `requestAnimationFrame`, the engine's event handlers, `engine.call`, the model
  updates and the forced layout reads, reporting counts and milliseconds per second, with a
  command channel to flip switches and drive pages in the live view.
- `ui_transitions.py`: the frames CSV lined up with the game log's page markers, the cost of
  every page switch as the worst frame and the total stall in the two seconds after.

## The engine's surface

Every menu, the HUD and the car displays are HTML documents rendered by Coherent Gameface,
`cohtml.WindowsDesktop.dll` 1.61.0.3 with V8 9.4.146 and the Renoir renderer, plain DLLs next
to the exe. The exe imports one entry point, `Library::Initialize(licenseKey, LibraryParams&)`,
and the constructors of the listener interfaces it subclasses, everything else is reached
through vtables: the library creates the system, the system creates four views (the 1920x1080
menu and HUD view and three car display views).

Layouts on 0.9.0, read from the exe's own set up code: `SystemSettings` is 0x80 bytes with
the resource handler at `+0x10`, the localization manager at `+0x20`, the image and SVG cache
watermarks at `+0x38` and `+0x48`, `int DebuggerPort` at `+0x68` (the game passes -1) and
`bool EnableDebugger` at `+0x6c`. `ViewSettings` has the listener at `+0x00`, width and height
at `+0x10` and `+0x14`, `EnableComplexCSSSelectorsStyling` true at `+0x30`. `LibraryParams` is
0xb0 bytes with the `OnWorkAvailable` callback at `+0x60` and its user data at `+0x68`, the
default font family at `+0x78` and an engine option string at `+0xa0`.

Vtable slots identified: `Library` 1 `CreateSystem`, 5 `ExecuteWork(type, mode, count)`.
`System` 3 `CreateView`. `View` 9 `Resize`, 16 `EnableRendering`, 27 to 30 the input events,
49 and 50 the cache limits the game sets after creation, 52 to 56 the binding calls, 60
`ExecuteScript`, 61 `AddInitialScript` (a script run at the start of every new document, the
injection vehicle that needs no file replaced), 62 `ResetInitialScripts`, 65 the custom effect
renderer the game installs, 72 and 73 the data bind model calls.

Threading: Cohtml creates no threads. Its layout and style work (type 1) and resource work
(type 0) are handed out through `OnWorkAvailable`, the game posts a job per notification, and
its render thread executes queued jobs while it waits, which is how the render thread came to
run the layout itself. The view only advances while the game window is active: when another
window takes activation the game keeps rendering but the UI's clock stands still and no frame
callback runs until a real input event reaches the window.

The inspector: with the port set, `http://localhost:<port>/json/list` lists the views, the
socket of a target is `ws://localhost:<port>/devtools/page/<id>` (the advertised URL is
wrong), `Runtime.evaluate` works and is enough to drive pages (`window.ksUI.goTo(page, path)`),
`Page.addScriptToEvaluateOnNewDocument` is accepted and inert, `Performance` is absent, and
the V8 sampling profiler (`Profiler.start`) crashes the game within seconds. Inside the pages
`performance.now()` is frozen for the whole frame and every frame callback receives its own
timestamp.

The pages: nineteen documents under `uiresources\` in the package, one 2.3 MB bundle
`js\components.js` shared by all (about two hundred custom elements registered through
`ksUI.registerModule`), one 1.1 MB stylesheet with 5,300 rules of which 764 use child or
sibling combinators. Every page switch is `window.location.replace`, every frame five data
models are fetched and marked wholly changed, every lazily loaded component runs its own frame
loop that reads its rectangle every 125 ms and moves its body in and out of the document, the
focus box is repositioned every 33 ms through four edge offsets, and the controls page builds
all rows of a group in one call.

## The overhaul round

On the owner's word one round of TODO-011 went into a build: the controls page rows built ten
per frame, the focus box placed with a transform and a size, the scrollbar thumb moved with a
transform with every style write guarded against unchanged values, and the two visibility
fixes underneath, all as an initial script of the menu view. The owner drove it: the lag was
the same and the controls list came up empty, because the patched row build read the filter
input's value before the input existed and threw. The round was removed with everything else.

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
replaced (TODO-011, dropped): rows built in slices across frames, restores and focus moves that
do not touch layout properties, the model refresh only when a model changed, one document kept
alive for the in race pages. Days of work against a 50,000 line bundle, with the risk shown on
2026-09-05 and again in the overhaul round, and no engine shortcut. Closed by the owner, the
mod carries nothing UI related (DEC-010).
