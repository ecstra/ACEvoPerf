---
name: cohtml-ui-engine
kind: doc
description: how the game's Coherent Gameface UI engine is wired, what the proxy can reach in it, the inspector, and how the UI's own pages and scripts behave
updated: 2026-09-06
links: [proxy-architecture, ui-lag-hunt-2026-09-06, BUG-014-ui-pages-lag-on-open-switch-and-interaction, moddability]
---

# The Cohtml UI engine

Every menu, the HUD and the car displays are HTML documents rendered by Coherent Gameface,
`cohtml.WindowsDesktop.dll` version 1.61.0.3 with V8 9.4.146 (`v8.dll`) and the Renoir
renderer (`RenoirCore.WindowsDesktop.dll`), all plain DLLs next to the exe. Read on 0.9.0 by
disassembly and by the proxy's hooks during the UI lag hunt (`ui-lag-hunt-2026-09-06`).

## How the exe reaches it

The exe imports one entry point, `cohtml::Library::Initialize(licenseKey, LibraryParams&)`,
plus the constructors of the listener interfaces it subclasses (`IViewListener`, `ILogHandler`,
`IPerformanceHandler`, `ILocalizationManager`, `IAsyncResourceHandler`). Everything else is a
C++ interface reached through vtables: the library creates the system, the system creates the
views. The proxy patches the import slot of `Library::Initialize` and follows the chain
(`src/ui/cohtml.cpp`), which gives it every object with a log line per step.

Structure layouts on 0.9.0, read from the exe's own set up code:

- `LibraryParams`, 0xb0 bytes: `+0x00` log handler, `+0x18` and `+0x20` file system reader and
  writer, `+0x40` to `+0x42` and `+0x80` to `+0x82` bools, `+0x60` the `OnWorkAvailable`
  callback with its user data at `+0x68`, `+0x78` the default font family, `+0xa0` an engine
  option string (the game passes `--enable-path-buffers-cache`).
- `SystemSettings`, 0x80 bytes: `+0x10` resource handler, `+0x20` localization manager,
  `+0x38` and `+0x48` image and SVG cache watermarks (1024 entries, 32 MB, 256, 8 MB),
  `+0x68` int `DebuggerPort` (the game passes -1), `+0x6c` bool `EnableDebugger`.
- `ViewSettings`: `+0x00` listener, `+0x10` and `+0x14` width and height, `+0x30`
  `EnableComplexCSSSelectorsStyling` true, `+0x44` double click time 200 ms.

Vtable slots that were identified: `Library` 1 `CreateSystem`, 5 `ExecuteWork(type, mode,
count)`. `System` 3 `CreateView`. `View` 9 `Resize`, 16 `EnableRendering`, 21 and 22 paint
rectangles and element boxes, 27 to 30 the key, gesture and mouse events, 49 and 50 the cache
count and byte limits the game sets right after creation, 52 to 56 the binding calls, 60
`ExecuteScript`, 61 `AddInitialScript` (a script run at the start of every new document), 62
`ResetInitialScripts`, 63 `TerminateScriptExecution`, 65 the custom effect renderer the game
installs, 67 `SetCustomMediaFeature`, 72 and 73 the data bind model calls, 74 `ReservedMethod`.

The game creates four views: the 1920x1080 menu and HUD view and three car display views
(1024x1024, 512x128, 512x512) whose pages come from `content\cars\<car>\displays\`.

## Threading

Cohtml creates no threads. Its layout and style work (work type 1) and its resource work (type
0) are handed out through the `OnWorkAvailable` callback, one notification per batch, and the
client executes them through `Library::ExecuteWork`. The game posts a job to its own scheduler
per notification, and because its render thread executes queued jobs while it waits, the
render thread ran nearly all of the layout work: on a heavy page 180 to 460 ms of every second,
single tasks up to 53 ms, CPU time equal to wall time. Running that work on a thread of the
proxy instead changed nothing, because `View::Advance` and the paint wait for the layout result
on the render thread anyway. The engine's internal switches (`--sync-layout-with-advance-waiting`,
`--sync-layout-with-start-of-advance`, `--synchronous-style-solving`,
`--disable-invalidation-sets`) are diagnostics that disable optimisations, not levers.

The view only advances while the game window is active. When another window takes activation
the game keeps rendering at full rate but stops advancing the UI, its `performance.now()` clock
stands still and no frame callback runs, until a real input event reaches the window. That is
what a "frozen menu that will not close" is after a window switch, and any tool that scripts
the UI has to send the window a key or a mouse nudge first.

## The inspector

`[ui] inspector_port=9444` in the ini makes the proxy pass the port and `EnableDebugger` in
`SystemSettings`. Then `http://localhost:9444/json/list` lists the views as page targets, the
protocol version is 1.3 and the socket for a target is `ws://localhost:9444/devtools/page/<id>`
(the advertised URL repeats the request path and is wrong). Domains present in the DLL:
Runtime, Debugger, Profiler, HeapProfiler, DOM, CSS, Network, Page, Console, Log, Overlay,
Input, Target, Schema, Animation, Storage, IO. `Performance` is not implemented.
`Runtime.evaluate` works and is enough to drive pages (`window.ksUI.goTo(page, path)`) and to
read anything. `Page.addScriptToEvaluateOnNewDocument` is accepted and does nothing, the view's
`AddInitialScript` is the working equivalent. The V8 sampling profiler (`Profiler.start`)
crashed the game three seconds in, on a V8 worker thread (access violation in `v8.dll`, 2026-09-06
10:30), do not use it. Chrome's own DevTools frontend can attach to the socket, the DLL does not
ship the inspector frontend files.

Inside the pages `performance.now()` is frozen for the whole frame and every frame callback
receives its own timestamp, so timing inside a frame needs `Date.now()` and counting frames
needs a callback registered first.

## The pages and their scripts

Everything is in `uiresources\` of the package (see `content-package`): nineteen page
documents (`menu.html`, `singleplayer.html`, `ingame.html` for the pit lane and pause menus,
`hud.html`, `settings.html`, `vehicles.html` and so on), one script bundle
`js\components.js` (2.3 MB, 50,000 lines, about two hundred custom elements registered through
`ksUI.registerModule`) shared by all pages, `js\cohtml.js` (the engine binding),
`css\uicomponents.css` (1.1 MB, 27,000 lines, 5,300 rules, 764 with child or sibling
combinators, 198 hover rules) and `css\ui.css`.

What the framework does that costs (details and numbers in `ui-lag-hunt-2026-09-06`):

- Every page switch is a full document reload through `window.location.replace`, so the bundle
  and the stylesheet are parsed and every component initialised again. `ksUI.goTo` only
  changes the path without a reload when the target is the current document.
- Every frame `perFrameAllModelUpdate` calls the game for five data models (`engine.call`,
  about 300 a second) and marks each whole model changed (`engine.updateWholeModel`).
- Every lazily loaded component (the rows of the controls page, many buttons) runs its own
  `requestAnimationFrame` loop that every 125 ms reads its rectangle and computed style and
  moves its body into or out of the document. The game starts a new loop on every connect and
  on every restore of a body and cancels only the last one, and `disconnectedCallback` returns
  before the cancel when the template never loaded, so loops leak.
- The focus indicator box is repositioned every 33 ms through four layout affecting style
  properties, hover focuses the element under the mouse, and the window `mousemove` handler
  blurs the active element after 6 px of movement.
- Wheel scrolling animates `scrollTop` over 450 to 600 ms and dispatches a scroll event per
  step whose handler reads sizes and writes the scrollbar thumb's height and top.
- The controls page builds all rows of a bindings group in one call (`populateBindings`,
  about 700 elements) and rebuilds on every group switch.
