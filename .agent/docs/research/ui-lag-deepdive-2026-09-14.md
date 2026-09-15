---
name: ui-lag-deepdive-2026-09-14
kind: doc
description: BUG-014's deep dive with no new run, the menu and HUD lag split into five separate costs, the controls page freeze as navigation scans in script, interaction as one Cohtml task that grows with the page, the pit menu and its pages updated one frame in three, document loads with stylesheets parsed again and vehicle setup built twice, and a light HUD, with what the mod's newer capabilities reach, twelve ranked fixes and one decisive developer build
updated: 2026-09-14
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three, BUG-025-controls-page-scans-the-page-once-per-new-row, BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open, BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load, TODO-027-the-ui-developer-build-and-one-session, DEC-019-ui-lag-work-reopened, ui-lag-hunt-2026-09-06, one-percent-lows-2026-09-14, package-override-layer, BUG-017-trackside-big-screens-blurry, TODO-011-ui-overhaul-through-injected-scripts]
---

# The UI lag, the deep dive

BUG-014's round of 2026-09-14, the owner having reopened the UI on 2026-09-13. Five angles from the exe,
the UI files in the package, Coherent's documentation and the hunt's history (the capabilities the mod
has gained, the HUD while driving, how the exe drives Cohtml, the menu pages, the Cohtml and V8
settings), each checked by a second agent, then a critic and three gap rounds (the rotation per surface,
what an interaction turn does, how the controls page freeze grows). No new session. RVAs are for the
0.9.1 exe and `cohtml.WindowsDesktop.dll` 1.61.0.3. Line numbers are for `uiresources/js/components.js`
of 0.9.1, the same file at every line the 0.9.0 sessions cite. The hunt's sessions are `logs/ui1` to
`logs/ui17`. The agents' scripts and extracted files lived in the session's scratch space and are not
kept.

## Five separate costs

1. **Clicking a bindings group on the controls page**, one frame of 250 to 500 ms, and one frame of 95 to
   139 ms when the page opens. Every new row asks the navigation library to scan the whole page once per
   navigation section, four of them, in one frame, 3.1 to 3.4 ms per row. It is script, not layout, and
   the scans cannot change anything because the new rows are still parked outside the document.
   [BUG-025](../../bugs/BUG-025-controls-page-scans-the-page-once-per-new-row.md).
2. **Interacting with a page** (hover, scroll, dragging a slider). Each UI update while the owner
   interacts runs one Cohtml style and layout task that grows with the number of elements on the page,
   about 33 ms on the main menu, 52 to 65 ms on the controls page and 60 to 90 ms frames on vehicle
   setup. The page scripts are not what makes it big. What Cohtml does inside that task is not known.
3. **The pit menu and every page opened from it** (vehicle setup, settings, controls). The engine updates
   the UI view there one frame in three, the same rotation it uses for the HUD while driving, because its
   "every view every frame" rule covers only the main menu showroom and pause.
   [BUG-024](../../bugs/BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three.md).
4. **Every change of document** (pause, resume, back to the pits, main menu pages) costs one 115 to
   207 ms frame and 250 to 670 ms of stall. Most of it is the page building its components again. Around
   that the game reads and parses 1.2 MB of stylesheets again on every load
   ([BUG-027](../../bugs/BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load.md)), and
   vehicle setup asks the game for the setup twice on every open
   ([BUG-026](../../bugs/BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open.md)).
5. **The HUD while driving** burns 125 to 190 ms of CPU a second across threads but only 5 to 9 ms a
   second on the render thread. It is a small part of BUG-009.

## How the exe drives the UI

- **The frame path.** EvoUi vtable slot 7 (`.rdata` `0x3173d90`) reaches `GameUi::PostFrame`
  (`0xde3650`), which posts one UI job every game frame. The job body `0xdd7940` runs input, then
  `SynchronizeModels` (View slot 69) for the listed views, then `System::Advance` every frame
  (`0xde2658`) and `View::Advance` (slot 6, `0xde26b7`) only for the listed views, then the paint
  (`0xdee230`, `Paint(frameId, waitForDrawDone = true)` at `0xdee3db`) for every view. EvoUi slot 8
  (`.rdata` `0x3173d98`) reaches `GameUi::EndFrame` (`0xde75c0`), which waits for that job at `0xde75e1`
  and runs queued jobs while it waits, which is how the render thread runs Cohtml's layout. Which thread
  calls slots 7 and 8 is still open.
- **The rotation and its flags.** PostFrame lists every view when `GameUi+0x555` or `+0x556` is set,
  otherwise only one view a frame in rotation, the mechanism and its weight in frame time being in
  [one-percent-lows-2026-09-14](one-percent-lows-2026-09-14.md). The flags have one writer (`0xdf3720`),
  reached only through EvoUi slot 24 (`.rdata` `0x3173e18`) from `GameScenePresenter::update` at
  `0x96b5cf`. `+0x555` is 1 only in `PaintShopGameModeClient`, the main menu showroom. `+0x556` is the
  pause flag (`PlatformCore+0x11a0`, set and cleared by utility commands 7 and 8) or a replay at speed 0
  or below. `+0x557` marks a replay and is read nowhere.
- **The rotation per surface.** 513 windows in 60 sessions on both versions, a heavy frame being over the
  larger of 40 ms and three times the window median.

| version | surface and page | heavy frames | gaps a multiple of 3 | gaps of 1 | heavy mean | median fps |
|---|---|---|---|---|---|---|
| 0.9.0 | main menu, settings and controls | 562 | 0.14 | 0.56 | 81.1 ms | 43.9 |
| 0.9.0 | pause, settings | 372 | 0.18 | 0.53 | 97.8 ms | 38.8 |
| 0.9.0 | pit lane, settings | 98 | 1.00 | 0.00 | 86.4 ms | 61.4 |
| 0.9.0 | pit lane, vehicle setup | 81 | 1.00 | 0.00 | 74.8 ms | 64.4 |
| 0.9.0 | HUD | 57 | 0.81 | 0.12 | 72.1 ms | 82.0 |
| 0.9.1 | pit lane main page | 31 | 1.00 | 0.00 | 44.3 ms | 83.9 |
| 0.9.1 | HUD | 70 | 0.93 | 0.00 | 74.7 ms | 81.5 |

  So pit pages tick 28 to 38 times a second idle and about 10 times a second while busy, input waits up
  to two frames and script delays counted in frames take three times as long. The main menu and pause
  never rotate. The HUD's 3 frame fold grows with the field, 0.35 to 0.60 ms solo and 0.63 to 1.48 ms in
  the 30 AI races.
- **Car displays never leave the rotation.** Their enabled byte is set at creation from the dashboard
  displays setting and nothing clears it. The pit pages have no opaque background, so the scene shows
  through. Whether the dash screens are on screen behind the pit menu is the owner's to say.
- **Mouse input reaches the main view every game frame.** The input step calls its snapshot diff
  (`0xdf20c0`) whenever the UI is loaded and the main view is in the frame, whatever the rotation, and
  sends at most the latest mouse move through View slot 29. A rotated view is handed up to three moves
  per turn.
- **The UI wait is BUG-009's job counter wait.** `0x27a15b0` runs `while counter > 0 RunOneJob`, whose
  job runner takes the scheduler's spin lock at `0x279fa90`. Its callers include the renderer's frame
  function (`0x1d88eb8`), `GameScenePresenter::update` (`0x96bd15`) and EndFrame (`0xde75e1`). A long UI
  job holds the frame in the same wait BUG-009's slow frames spin in.
- **The Cohtml build and the injection slot.** The library, system and view vtables (cohtml
  `0x63A440`, `0x63C168`, `0x63E208`) match the hunt's 0.9.0 logs. View slot 61 (`0x479ce0`) copies a C
  string into a vector at `View+0x1c18`, the shape of `AddInitialScript(const char*)`, and slot 60 enters
  the V8 isolate.
- **The old hooks had wrong signatures.** `OnWorkAvailable` on 0.9.1 is `(userData, type, family)`, and
  type 1 work runs `ExecuteWork(1, 0, family)`. The hunt's code in `f4ab7fc` declared the callback with
  two arguments and named the third `ExecuteWork` argument a count, so it must not be reused.
- **Model sync is pulled by the page.** Each view's `perFrameAllModelUpdate` calls `engine.call('get' +
  name)` for every enabled model on every frame of that view and replaces the whole model with the
  result. The HUD pulls 9 models, about 250 calls a second at 27 to 29 turns a second, and each car
  display pulls the current car model. The 33.3 ms gate is commented out by Kunos (lines 4252 to 4262,
  and line 3011 of the displays script).

## The settings the game passes

- `EnableComplexCSSSelectorsStyling` is on, and 4,771 of 5,384 rules need it (88.6 percent), because
  Cohtml counts the descendant combinator as complex. Turning it off is dead.
- Async style solving is switched off for the life of the menu and HUD view at the first `hud.html`
  load, by the rev dial setting `cohCustomEffectName`, one log line per race process. Coherent fixed the
  "never comes back" part in 1.65 and 1.68. It does not make menu stalls worse, and pit lane interaction
  turns are lighter with it off (vehicle setup heavy frames 82.0 and 81.1 ms with it on, 68.5 and 72.8 ms
  with it off).
- A GC idle deadline of 0.5 ms per Advance, one V8 worker hint, no V8 flags from the game, and
  `--enable-path-buffers-cache` as the only engine option.

## Document loads

- **What a load costs on 0.9.1**, from frames around every stylesheet reread in the 30 AI races of
  2026-09-13.

| session | page | loads | median worst frame | median stall | largest stall |
|---|---|---|---|---|---|
| fix1-ai30 | hud.html | 16 | 146.5 ms | 639.5 ms | 792.2 ms |
| fix1-ai30 | ingame.html | 15 | 125.6 ms | 264.6 ms | 652.8 ms |
| census-ai30 | hud.html | 12 | 153.6 ms | 670.8 ms | 1066.6 ms |
| census-ai30 | ingame.html | 15 | 125.7 ms | 264.3 ms | 647.0 ms |

  The HUD's stall includes the step from paused to driving frame times, so it is an upper bound.
- **Only a change of document reloads.** `ksUI.goTo` (line 5929) ends in `window.location.replace` or
  `location.reload` only when the target document differs. Pages inside one document, the pit lane to
  vehicle setup inside `ingame.html` or settings to controls inside `settings.html`, are model swaps.
- **The stylesheets are parsed again every time.** `uicomponents.css` (1,153,706 bytes) and `ui.css`
  (48,807 bytes) are reread at every document load, 33 and 34 times in the 20 minute fix1 session, 38.2 MB
  of repeated reads, while `components.js` is read once per process and the game preloads `.js` files at
  startup (`0xDEF6D0`). The parse error near `-2ren` is logged after every page load.
- **Where the stall frame goes.** In 40 loads the game's DOMContentLoaded warning lands 5.6 to 24.7 ms
  into the frame that holds it, and 90 to 190 ms of that 115 to 207 ms frame comes after it, so compiling
  and running the top level of the 2.3 MB script fits in about 25 ms at most. A reload decomposes into
  about 50 ms of teardown over budget, 0 to 45 ms of CSS parse and 110 to 230 ms of module evaluation and
  first build. The one document shell of 2026-09-05 swapped pages in 4 to 11 ms and still measured 150 to
  400 ms of stall after the switch, and crashed once, so the build inside the page is the dominant cost.

## The controls page

- **Rows per group.** Car 81, Car_Advanced about 105, Camera about 74, UI about 29, Showroom 12.
- **The switch freeze is the third frame after the populate, linear in the rows.** 60 switches in ui3,
  ui4, ui7 and ui13.

| group | rows | clean median of that frame | ms per row over an 11 ms baseline |
|---|---|---|---|
| Showroom | 12 | 50.6 ms | 3.30 |
| UI | 29 | 85.0 ms | 2.55 |
| Camera | 74 | 260.9 ms | 3.38 |
| Car | 81 | 274.0 ms | 3.25 |
| Car_Advanced | 105 | 336.6 ms | 3.10 |

  A straight line fits 23 clean switches at 3.12 ms per row (R squared 0.973) far better than a square,
  and the script time per switch in ui17 has the same slope. The populate frame itself is only 17 to
  52 ms, so building rows in slices, the overhaul round's design, could not have helped.
- **Why, from the code.** Every row is lazily loaded, its body kept in a DocumentFragment (lines 1014 to
  1016) until the visibility loop restores it at least 125 ms later (lines 7283 to 7302). Its
  `setupNavigation` (line 7941) runs one and then three frames after connect (lines 8187 to 8221), and
  through `refocusNav` (line 7870) calls `SpatialNavigation.makeFocusable()` with no section, which
  queries `.focusable` once per section (line 3274), all four sections using the same selector. At that
  frame no new row is restored, so each of the N calls scans the same elements and cannot add a single
  `tabindex`.
- **It is script, not Cohtml layout.** Type 1 work regresses at minus 0.02 times the excess of the
  navigation setup frames against 0.62 times the excess of every other frame, over 62 windows.
- **The page open carries the same cost one frame later.** `populateBindings` runs inside an animation
  frame callback at an open, so the list rows' setup lands in the fourth frame (95 to 139 ms), about 1.0
  to 1.6 ms per row because fewer focusables are restored yet. The third frame then holds only the 20
  driving panel rows, a predicted ratio of 0.26 against 0.22 to 0.34 observed.
- **From the pit lane the same page runs one UI frame in three**, and the list rows' setup there cost
  290 ms (ui3 at 10:42:32), 3.4 ms per row, because restores had already enlarged the page.
- **Scrolling runs the same scans.** In ui12 restores took 245 to 408 ms of script a second, and each
  restore calls `setupNavigation` at once and again a frame later. That the scans are most of it is not
  shown.

## Interaction

- **Hovering focuses nothing.** `_handleMouseOver` (lines 6962 to 6967) is registered only inside
  `bindFocusHandling` (line 6969), which nothing calls. In mouse mode focus comes only from a click on a
  focusable element, a slider drag or the wheel on the active slider. A drag gives one focus and one blur
  per value step.
- **Tooltips are cheap and stay in the page.** At most 11 `ShowTooltip` triggers in any logged second,
  16 slow turns in ui2 with none, and the exe holds no `ShowTooltip` string, so they reach only the
  control hints handler in the same view.
- **The class removal on every mouse move is free.** Cohtml's `DOMTokenList.remove` returns before any
  mutation when nothing was removed.
- **The focus box leaks an animation frame chain per focus change** (line 5012), up to 19 on the controls
  page, at about one rect read per turn whatever the count, and the box is hidden in mouse mode.
- **The slow interaction turn is one Cohtml task that scales with the page.** On the ui13 controls page
  after the last switch, with no new elements, type 1 work took 118 to 640 ms a second with single tasks
  of 51.9 to 65.5 ms, while script in frame callbacks took 0 to 40 ms. The 542 element main menu showed
  single tasks of 32 to 35 ms, and 1,313 element vehicle setup 75 to 90 ms frames. Idle pages take 0.1 to
  0.6 ms a second. That is about 50 to 60 microseconds per element in either style solving mode, and the
  165 selectors with a state pseudo class all sit on component or id bases, so broad selectors do not
  explain a whole document pass. That the hover state change starts it is inference by elimination.
- **Layout reads do not force layouts in Gameface.** `getBoundingClientRect` and `getComputedStyle` cost
  1 to 3 ms for 700 to 1,600 calls a second, and Gameface returns the last solved values. The hunt's
  "5,000 to 16,000 forced layout reads a second" forced nothing.

## Vehicle setup

- **It asks for the setup twice on every open.** `CarSetupRequestInit` appears twice a few milliseconds
  apart on every visit in five game logs of both versions (ui3 10:45:18.443 and .445, ai30-A-fix-on
  13:13:48.999 and 49.001). Each response rebuilds all 18 setup groups and 4 info panels with 36 whole
  page navigation scans and adds 36 change listeners per slider. The open frames were 237.8 ms then
  80.0 ms in ui3, with 697 new elements in the first second.
- **The interaction bursts are live on 0.9.1** and are not the setup apply. ai30-A-fix-on at 13:13:51.8
  to 13:13:56.4 logged 20 hitches of 60.6 to 93.1 ms with no streaming, starting before the first apply
  line, and ui3 shows a burst with no apply at all. Each burst frame is one main view turn followed by
  two display frames of about 10 ms.
- **The hunt's "12 to 29 fps" is the page's own tick rate.** The probe counted animation frame ticks in
  the page, 19.6 on vehicle setup and 30.2 on the pit lane page against 72 to 74 in pause and settings,
  while the 0.9.0 pit lane windows presented at a median of 81.8 fps.

## The HUD while driving

- **The "cohtml plus 743 in pure driving" reading is wrong.** It is lap 13's cumulative sampler build,
  see [one-percent-lows-2026-09-14](one-percent-lows-2026-09-14.md). Lap 14's real driving minute has no
  cohtml row among its ten largest excesses.
- **The render thread's UI share** on the 0.9.0 laps 10 to 17 is cohtml 4.0 to 7.3 ms a second, V8 0.47
  to 0.83 and Renoir 0.74 to 1.49, explaining 6 to 16 percent of the slowest frames' excess. Across all
  threads (the load sampler of 2026-09-12) the UI takes 124.5 to 188.2 ms of CPU a second.
- **A model gate would cut about 22 percent of the pulls, not 65.** With the view advancing one frame in
  three, its median turn interval over 100 HUD windows is 35.5 ms, so a 33.3 ms gate still lets 78 percent
  of turns fetch.
- **Per frame layout writes.** The track map reads its size and writes `left` and `top` in percent for
  every car every frame (about 250 style writes a frame in a 30 car race), the delta bar writes its width,
  the radar reads sizes when cars are near, and the Ferrari display rewrites `innerHTML`. Not measured.

## What the mod's newer capabilities reach

- **Reach the UI.** EvoUi slots 7, 8 and 24 are plain `.rdata` pointers that `HookVtableSlot`
  (`src/core/iat.cpp`) can wrap. `View::Advance` can be timed per view. The rotation flags can be read
  and set. Page scripts can be patched through the package override layer, proven in game by the big
  screens fix (BUG-017), or through `AddInitialScript`, slot 61 checked on 0.9.1. The streaming trace
  already marks every document load.
- **Reach nothing in the UI.** The bundled DirectStorage core (1.2 MB per load on the file to memory
  queue), the texture streamer fixes, Reflex (it sleeps after present) and the command list hooks (the
  Renoir backend is inside the exe). A same name proxy for `cohtml.WindowsDesktop.dll` or `v8.dll` does
  not work, both are static imports mapped before the mod loads, and import table hooks already reach
  every function.
- **The owner's words.** "The new render refresh fix" and "a new injection" were matched to the streamer
  reload fix and the hash checked exe patches with medium confidence. The owner may mean something else.

## Killed

- Building the controls rows in slices, the populate frame is 17 to 52 ms and ui17 moved none of the
  3.13 ms per row.
- The leaked `ui.action` listeners slowing switches over time, Car_Advanced stays at 305 to 380 ms over
  about 20 switches.
- A hover focus and blur ping pong, a tooltip round trip through the exe, a restyle from the class
  removal, the leaked focus box chains, and broad `:hover` selectors, each shown above.
- Forced layout reads as the cause of relayouts, and transforms or read and write batching as fixes,
  measured in the hunt with no change and reads force nothing.
- Async style solving as the cause of stalls, and turning off complex selectors.
- Pruning the stylesheet, 366 of 5,261 rules provably dead, 43 KB of 708 KB.
- Skipping a model update in the exe, the pull and the whole model replace live in the page script.
- The main view every frame on every surface, it triples HUD work while driving to remove a 0.58 ms
  ripple and changes nothing in the main menu and pause.
- Skipping Advance on alternate frames, the HUD already advances one frame in three.
- Separate laps with `no_hud` and `no_dash` to split the HUD from the dashboards, removing views changes
  the others' advance rate.
- One document kept alive for the in race pages, measured on 2026-09-05 with the stall unchanged and one
  crash.
- V8 GC flags as a HUD fix, any V8 sample appears in only 4.9 to 9.4 percent of the slowest frames.

## Fixes, ranked by what the owner would feel against the risk

1. **Controls page, skip the scans that cannot change anything** (BUG-025). Patch
   `KsHTMLElement.prototype.setupNavigation` so that when the element is navigable, not clearing, lazily
   loaded, still in its fragment and its section exists, it returns after at most one `ui.action` listener, and fold
   every other call into one stock `SpatialNavigation.makeFocusable()` on the next animation frame
   (trailing, because axis rows add buttons without calling it, lines 37550 to 37619). A safe partial with
   no behaviour change is one scan per distinct selector and ignore list instead of per section. Wrapped
   in try with the stock method as fallback, loaded before `components.js` through the override layer or
   `AddInitialScript`. Expected switch freezes of 197 to 494 ms down to about one normal frame, the open's
   setup frame to about 15 ms, and shorter scroll hitches. Risk low, a pad row never getting its
   `tabindex`, which the trailing scan covers. About 30 lines of script.
2. **The pit menu counts as a menu** (BUG-024). A wrapper on EvoUi slot 24 passes the flag argument as 1
   while the main view's document is not `hud.html`, the path pause already uses. A budget variant keeps
   the main view every frame and rotates only the displays, a stub at `0xde379b`. Idle and light pit pages
   would tick at the frame rate (84 to 115 Hz instead of 28 to 38) and input would reach the menu up to two
   frames sooner. On heavy pit pages busy seconds would go from 54 to 60 frames with a menu turn every
   55 ms to about 31 to 33 frames with a turn every 32 ms, a trade the owner judges. It changes an update
   rate, so it stands as correcting the engine's missing case of a menu open over a live session, not as a
   knob. About a day.
3. **Vehicle setup asks once** (BUG-026). `VehicleSetupPage.prototype.init` ignores a call while its own
   request is outstanding. The open's work should halve, and fix 1's fold removes its 36 scans. An hour.
4. **Stylesheets preloaded once** (BUG-027). After `Library::CreateSystem` returns, call the system's
   `PreloadAndCacheStylesheet` (slot 22) for both sheets. Removes the repeated reads and a full parse per
   load. Medium risk, Coherent fixed a reuse failure in 1.65 and a crash on removing a `<link>` to a
   preloaded sheet with media rules in 3.1.1, and `uicomponents.css` has two `@media` blocks. Proven when
   the `-2ren` parse error appears once per process, killed by an unstyled page or a crash on the first
   switch. About a day.
5. **HUD layout writes as transforms with cached sizes**, the track map, radar and delta bar. A BUG-009
   lever more than a felt menu fix, worth building only if the HUD turn is over 2 ms in a 30 AI race.
6. **A code cache for `components.js`**, bounded at about 25 ms per stall frame, kept only if a cache is
   offered and rejected on every load.
7. **Skip hidden car display turns**, only if the dash is not visible behind the pit menu or pause.
8. **Script churn guards**, the blur while the pointer is inside the focused element, repeated identical
   tooltips, the leaked focus box chain. Real defects, no felt change expected.
9. **A template parse cache in `loadResource`**, small since populate frames are 17 to 52 ms.
10. **HUD model gates**, about 22 percent fewer pulls, and a 30 Hz HUD is a rate cap unless the values only
    change that often.
11. **Skip the per frame target copies of views that did not paint**, a small GPU saving, shared with
    BUG-009.
12. **Decouple the UI from the frame**, compositing the last UI image while its job is still busy. The
    biggest possible effect, high risk (command list ownership, render target states, input), days of work,
    and it would skip the scheduler wait whose spin lock patch once made loads worse. Parked until the
    developer build shows how much of each stall is that wait.

## The decisive build

One developer build, off by default, and one session of about 15 minutes, filed as
[TODO-027](../../todos/TODO-027-the-ui-developer-build-and-one-session.md). It replaces more than ten runs
the five angles proposed.

- **Engine side.** The Cohtml hook chain as in `f4ab7fc` with the real signatures, `ExecuteWork` timed per
  thread and type, `View::Advance` wrapped per view (keyed by view size) with the calls, total and largest
  milliseconds a second, EvoUi slots 7 and 8 wrapped with their thread ids and slot 8's wait per frame in
  the frames CSV, EvoUi slot 24 wrapped logging its flag arguments and forcing the menu flag on in 20 s blocks
  that alternate with off, the rotation index per frame, and the V8 module compile wrapped for source length, cache offered,
  cache rejected and milliseconds.
- **Page side.** An initial script through slot 61 on the menu and HUD view, logging once a second the
  `setupNavigation` calls, skipped calls and milliseconds, the focusable count, focus and blur counts,
  tooltip counts, and mouse events per view turn binned 0 to 4 or more. Fix 1 and the vehicle setup guard
  switch together by page entry parity kept in `localStorage`, which survives document loads. The script
  stops itself when its page goes away, and the V8 profiler is never used.
- **The owner's session.** Controls page group clicks from the main menu four times, a slow hover, a wheel
  scroll and a keyboard walk down the Car list, the pit menu idle for 40 s, vehicle setup hover and drag
  four times, controls from the pit lane, two laps, controls from pause.
- **What it decides.** Fix 1 proven if stock Car switches show about 81 calls of 2.5 to 4 ms and fixed
  entries show no frame over 30 ms with every row still reachable by keyboard, killed if fixed Car_Advanced
  stays over 150 ms. Fix 3 proven if fixed opens log one request and a third less Advance time. The pit
  menu flag proven if forced windows lose the 3 frame fold, worth building if the owner calls them smoother.
  The interaction task named, per element cost against script and event time. The code cache kept or
  killed. Which thread calls slots 7 and 8, and how long slot 8 waits in the slowest frames on the laps.

## Still open

- What the 50 to 60 microseconds per element interaction task is inside Cohtml, from the build's markers
  or from the writer of the hover flag (near cohtml `0x639C31`) statically.
- Whether Cohtml runs mouse event handlers inside the mouse event call or on the next Advance.
- Which thread calls EvoUi slots 7 and 8, since static search by call pattern found nothing.
- Whether any code cache reaches Cohtml's module compile today.
- Whether `EnableUISurfacePartitioning` (View slot 20) would cut full surface redraws when only part of
  the HUD changes.
- Whether the dash screens are visible behind the pit menu and pause, the owner's to say.
- Leak shaped facts for BUG-016: every setup slider change leaves 35 unresolved promises, a second vehicle
  setup visit keeps the first visit's 22 handlers, rows removed before their template loaded keep their
  visibility loop (loop callbacks grew from 173 to 583 a frame over about 12 switches in ui13), and nine
  Noto Sans SC font files (95 MB) are read at startup in an English session.
- BUG-012's 1.3 to 1.5 s pit lane return freeze is larger than any document load measured here, so it is
  not the UI.

## What it corrects

- [ui-lag-hunt-2026-09-06](ui-lag-hunt-2026-09-06.md). "5,300 rules of which 764 use child or sibling
  combinators" is 5,173 style rules in `uicomponents.css` with 871 selectors using a child or sibling
  combinator, and 4,771 of 5,384 rules across both sheets complex by Cohtml's definition. "Vehicle setup
  at 12 to 29 fps" is the page's tick rate on a rotated view. The forced layout reads force nothing. "None
  reachable from the engine side" is out of date. The synchronous list builds are 17 to 52 ms and the
  freeze is the navigation scans three frames later. "Focus and blur storms" and "hover focuses the element
  under the mouse" are wrong. `ExecuteWork`'s third argument is a family, not a count. "Every page switch is
  a document reload" holds only for a change of document, and the models fetched are five in the menus and
  nine on the HUD, per frame of the view. The frame waits for the whole UI job, not only for layout, which
  is why moving layout to a mod thread changed nothing.
- [TODO-011](../../todos/TODO-011-ui-overhaul-through-injected-scripts.md) item 1, "about 100 rows, 260 to
  360 ms in one frame", is not the row build.
- [optimisation-deepdive-2026-09-12](optimisation-deepdive-2026-09-12.md), cohtml as the biggest excess in
  slow driving frames, corrected there.
