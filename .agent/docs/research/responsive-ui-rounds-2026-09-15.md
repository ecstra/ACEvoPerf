---
name: responsive-ui-rounds-2026-09-15
kind: doc
description: the UI lane's fix rounds of 2026-09-14 and 2026-09-15, seven probe laps from the first measured build to the shipped responsive UI, what each lap showed, the numbers before and after every fix, the probe's own stalls, and what page opens still cost
updated: 2026-09-15
links: [responsive-ui, ui-lag-deepdive-2026-09-14, ui-lag-hunt-2026-09-06, BUG-014-ui-pages-lag-on-open-switch-and-interaction, BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three, BUG-025-controls-page-scans-the-page-once-per-new-row, BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open, BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load, BUG-028-page-opens-still-hold-frames-of-100-to-200-ms, TODO-027-the-ui-developer-build-and-one-session, DEC-019-ui-lag-work-reopened, DEC-020-responsive-ui-is-one-switch-on-by-default, telemetry]
---

# The responsive UI rounds

The UI lane of DEC-019, worked on 2026-09-14 and 2026-09-15 after the deep dive
([ui-lag-deepdive-2026-09-14](ui-lag-deepdive-2026-09-14.md)) with the developer UI probe (`[developer]
ui_probe`, see [telemetry](../ops/telemetry.md)). The owner drove every lap on the reference laptop,
Assetto Corsa EVO 0.9.1 and Cohtml 1.61.0.3. Session folders are `logs/ui-probe-a-20260914` to
`logs/ui-responsive-g-20260915`. What shipped is described in [responsive-ui](../systems/responsive-ui.md).

## The laps

| Lap | Build | Owner's reading | What the logs showed |
| --- | --- | --- | --- |
| A, B | the probe, the controls page and vehicle setup fixes on alternate visits | hover in vehicle setup, the controls page open and its sliders slow, everything falls apart under fast input | the fixes work in script, the lag sits in Cohtml's style and layout task |
| C | restyle timing, invalidation marks per element | "done" | each hover change restyled 1,100 to 1,300 elements for 50 to 65 ms, from ancestors flagged as hover dependent |
| D | the sibling walk skip alone | "Entire thing is still laggy. Last fix attempt." | no change, the flagged containers were the amplifier |
| E | the narrowed stylesheet with the skip | "holy shit that worked" | hover, scrolling, sliders and switching smooth in the menus, session menus at a third of the frame rate, controls sliders still slow |
| F | the menu refresh fix, the controls refresh coalescing | "holy moly it works!" | session menus every frame, page opens the one thing left |
| G | the responsive UI, style matching and the resource work move | "works enough i guess" | style work per element halved, page opens shorter, no probe stalls |

## Hover, scrolling and sliders

Probe C timed every restyle and counted the nodes each invalidation marked. A hover change invalidated
every ancestor with Cohtml's state dependent flag (`+0x168`, set by the matcher at `0x3EF737`), and each
invalidation marked that ancestor's whole subtree, so one hover restyled the page. The flag came from four
generic selector parts, `div:hover`, `div:focus`, `.component-body:hover` and `.component-body:focus`, which
match nearly every container once the state is ignored. Lap D, the sibling walk skip on its own, changed
nothing. Lap E served the stylesheet with the parts narrowed and kept the skip, and the owner felt the
menus turn smooth.

## Menus in a session

After lap E the owner felt every in session menu capped at 30 to 60 fps. `GameUi::PostFrame` updates one
UI surface a frame outside the main menu and pause, so the menu took one frame in three against the two
dashboard displays. In lap G's session the main view advanced 80 times a second and each display 40 times,
at 80 fps.

## The controls page sliders

While a slider on the controls page was dragged the game sent `InputConfigurationResponseRefresh` with
`is_soft_set` 24 to 34 times a second, and each rebuilt every binding row and slider. Coalescing soft
refreshes to one every 100 ms made the sliders as smooth as the other menus'. Lap G counted 16 refreshes run
and 24 held.

## Pages opening

Lap F's logs split the open of a page into its frames. The settings page from the main menu held frames of
105.5 and 165.9 ms, the first for the new document's script (59 ms of Advance), the second for the controls
page build (39 ms of Advance and 123 ms of style, layout and paint). Three costs stood out.

- **The listed rules.** The layout sampler found Cohtml's loop over the listed rules (`0x3EEDF2`) on the
  stack in 40 to 67 percent of all layout samples, on every page. The list holds 2,142 of the game's 5,978
  selectors, 1,411 starting with a tag and 673 with an id, and each element ran the full matcher on all of
  them. For custom elements each tag check also copied and upper cased the element's name, and the copy,
  the compare and the heap calls around them (`0x1AE510`, `0x1AE590`, `0xDEBA0`, `0x3F9D70`, `RtlAllocateHeap`
  and `RtlFreeHeap`) were 4 to 19 percent of the innermost samples of each report, 0 to 9 in lap G. Those
  sums cover only each report's twelve largest entries.
- **Resource work on the frame thread.** On some loads the frame thread picked up the stylesheet parse
  while waiting, 57 ms of resource work on it in the settings open second and a 46.8 ms frame during the
  single player page's parse.
- **The probe itself.** 15 `[crash] Exception Detected` events in the game log, all in the probe reading
  nodes already taken out of the page, each stalling its thread 120 to 210 ms while the game's crash logger
  wrote a symbolised stack. Four of them fell within a second of the pit lane menu opening. Laps C and D
  had 856 and 678.

Lap G, measured against lap F.

| Measure | Lap F | Lap G |
| --- | --- | --- |
| incremental restyles, per node | 14.0 µs | 6.3 µs |
| restyles over 300 nodes, per node | 44.9 µs | 24.1 µs |
| whole document restyles, mean | 17.4 ms | 8.6 ms |
| listed rule loop on the stack | 40 to 67 % | 34 to 46 % |
| style work in the settings and controls open second | 211.7 ms | 76.5 ms |
| resource work on the frame thread, worst second at an open | 57.4 ms | 0.1 ms |
| probe stalls in the game log | 15 | 0 |

Page opens, time over 16.7 ms in the 1.5 s after the game's `Loading page` line and the worst frame.

| Page | Lap F | Lap G |
| --- | --- | --- |
| settings and controls from the main menu | 451 ms, 165.9 ms | 328 ms, 125.7 ms |
| main menu from settings | 235 ms, 87.3 ms | 220 ms, 78.6 ms |
| single player | 185 ms, 82.4 ms | 175 ms, 75.8 ms |
| pit lane menu from single player | 234 ms, 204.8 ms | 215 ms, 159.8 ms |
| pit lane menu from settings | 170 to 294 ms, 149.0 to 163.1 ms | 241 ms, 164.9 ms |

## What page opens still cost

- **The page's own script.** The frame a page appears in runs its module and component set up, 40 to 68 ms
  of Advance in lap G, before any style work.
- **Pages built over several frames.** Settings, video in lap G added rows into a scrolling container over
  a second, 52 child list changes on the container each marking its subtree, 30,432 marks and 40,627 nodes
  restyled in that second with a 201.5 ms frame. How Cohtml's child list invalidation (kind 0) picks what
  to mark is not read yet.
- **The listed rules still walked.** The loop still visits all 2,142 rules per element to skip them, 7 to
  14 percent of layout samples in the stub itself. Filing those rules by tag and id would remove the walk,
  with an index the rule set's add and remove (`0x3E9C40`, `0x3F0100`) would have to keep current.
- **The stylesheets parsed at every load** (BUG-027), now off the frame thread.

These are BUG-028. Cohtml's shutdown asserts about fonts in lap G's log (`Font clients should have been
unregistered`) also appear in 22 earlier sessions, the oldest the load sampler run of 2026-09-12, and are
the game's.
