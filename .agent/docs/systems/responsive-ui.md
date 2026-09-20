---
name: responsive-ui
kind: doc
description: the responsive UI, the one switch that keeps the game's menus smooth and the HUD from uneven driving frame times, its parts, where each lives, what each patches and how it checks the build first, and the shared Cohtml hooks it and the UI probe stand on
updated: 2026-09-20
links: [review-2026-09-fix-review-cohtml-build-guard, responsive-ui-rounds-2026-09-15, ui-lag-deepdive-2026-09-14, BUG-014-ui-pages-lag-on-open-switch-and-interaction, BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three, BUG-009-one-percent-lows-far-below-average, TODO-025-the-ui-view-rotation-test, BUG-025-controls-page-scans-the-page-once-per-new-row, BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open, DEC-020-responsive-ui-is-one-switch-on-by-default, package-override-layer, telemetry, proxy-architecture]
---

# Responsive UI

`[engine] responsive_ui`, on by default, one switch for every UI fix (DEC-020). The game's menus, the
HUD and the car displays are HTML documents rendered by Coherent Gameface, `cohtml.WindowsDesktop.dll`
1.61.0.3. Every part below checks the build it was read from before it changes anything, and a game or
UI engine update leaves that part out with a log line. The measurements behind each part are in
[responsive-ui-rounds-2026-09-15](../research/responsive-ui-rounds-2026-09-15.md).

`InstallResponsiveUi` in `src/ui/responsive_ui.cpp` runs from `DllMain` and installs the engine patches,
then registers with the shared Cohtml hooks. The stylesheet part runs from the overlay.

## The parts

| Part | Code | Log | What it changes |
| --- | --- | --- | --- |
| Narrowed stylesheet | `AddUiStyleFix` in `src/overlay/overlay.cpp` | `overlay: UI stylesheet, 7 hover and focus selector parts narrowed` | serves `uiresources\css\uicomponents.css` with seven generic `:hover` and `:focus` selector parts narrowed |
| Restyle fix | `src/ui/restyle_fix.cpp` | `[restyle]` | skips the sibling walk of a state change's invalidation |
| Menu refresh fix | `src/ui/menu_refresh_fix.cpp` | `[menus]` | a menu page and the HUD in a session update every frame, the car displays take turns |
| Style matching fix | `src/ui/style_match_fix.cpp` | `[styles]` | elements skip rules they cannot match, custom element names are compared in place |
| Child removal fix | `src/ui/child_removal_fix.cpp` | `[children]` | removing a child restyles only the children whose rules look at their position |
| Page fixes | the script in `src/ui/responsive_ui.cpp` | `[responsive ui] page fixes added` | the controls page's navigation scans, vehicle setup's double init, the controls page refresh storm |
| Resource work move | `src/ui/responsive_ui.cpp` | `[responsive ui] resource work` | resource work the frame thread picks up runs on a mod thread |

### Narrowed stylesheet

Cohtml flags an element as state dependent when a selector part with `:hover` or `:focus` matches it with
the state ignored, and on every hover change it restyles each flagged ancestor's whole subtree. The game's
`div:hover`, `div:focus` (paint shop page buttons) and `.component-body:hover` and `:focus` parts (grid
items, the material editor) flagged every container. The overlay reads the stylesheet from the player's
own package at start, checks that each of the seven parts occurs exactly as often as expected, narrows
them to what the styled elements always carry (`data-page`, `focus-indicator`, the channel group's own
hover) and serves the result from `acevo_uicomponents.css` next to the exe, the way it serves the big
screen fix (see [package-override-layer](package-override-layer.md)). A stylesheet an update changed is
served untouched.

### Restyle fix

The invalidation at cohtml `0x37B690` walks every sibling after the changed element whenever the page has
any sibling combinator rule, whatever feature changed. The game has one such rule and none with a state
pseudo class left of `+` or `~`, so a stub at the gate `0x37B92B` skips the walk for state changes (kind 5)
and keeps it for classes, attributes and ids. Three code regions are hashed first.

### Menu refresh fix

`GameUi::PostFrame` updates every UI surface each frame only in the main menu showroom and pause, and one
surface a frame in turn otherwise, the menu and the car's two dashboard displays, so a menu in a session
ran at a third of the frame rate. A stub at `0xDE37B7` keeps the main surface in every frame and passes the
turn between the displays. Which page is shown comes from a hook on Cohtml's URL loader (`0x46B990`), whose
stub records whether the main view shows one of the known menu pages or `hud.html`.

The stub reads a schedule byte, 0 the game's rotation, 1 the main surface every frame with the displays
taking turns, 2 every surface every frame, the path the game jumps to itself from its main and pause menu
tests. A menu page and the HUD both write 1. The HUD had the game's rotation until the run of 2026-09-15
showed it widening driving frame times, a 1 ms ripple every third frame at the Red Bull Ring GP that the HUD
every frame removed for 0.02 ms a frame, with the 1 percent low 7 fps higher (BUG-009). Under the developer
test `[developer] hud_schedule_test=1`, `MenuRefreshTick`, called once a second from the timeline thread,
moves the HUD through 0, 1 and 2 in shuffled turns of 10 seconds and logs each turn as
`[hud test] t=<seconds> schedule <name>` and each page load as `[hud test] t=<seconds> page <url>`.

### Style matching fix

Cohtml files each rule by the first simple selector of its rightmost compound. Rules that start with a tag,
an id or anything but a class land in one list (`+0x128` of the rule set, filed at `0x3E9C40`) that the
collector at `0x3EEC00` runs the full matcher over for every element, 2,142 rules in the game's stylesheets.
Checking a tag against a custom element (`ks-` elements) calls the element's name getter (`0x1AE4C0`), which
copies the name into a new string and upper cases it before the compare. Five stubs, each replacing whole
instructions and returning to Cohtml's own code:

- the listed rule loop (`0x3EEDF2`) skips a rule whose first simple selector is a tag or an id the element
  does not have, the matcher's own first check
- the matcher's custom tag compare (`0x3EDA76`) and the lone tag rule loop (`0x3EE9DF`) read the name the
  element keeps at `+0x238` when its name getter is `0x1AE4C0`, and compare it with the same compare, which
  folds case on both sides
- the two loops over rules with a state pseudo class (`0x3EF6D1`, `0x3EF760`) skip a compound whose first
  simple selector is a tag or an id the element does not have

What is matched does not change, only the calls that would have failed are not made. Nine code regions are
hashed first, and each stub has unwind data registered with `RtlAddFunctionTable`, copied from the frame of
the function it stands in, so stack walks and the UI probe's sampler pass through it
(`StyleMatchFixUnwind`).

### Child removal fix

When an element loses a child, the removal (`0x38FCB4`) invalidates the element for its changed child list
(slot 49, kind 0). That marks the element and every node under it, because the feature set's constructor
(`0x3E8C10`) builds the child list invalidation set to match every node whatever the stylesheets hold, and the
style update restyles all of them. A HUD part hidden with `data-bind-if` restyled the whole HUD when it went
away, 879 nodes and 21 to 24 ms for the wrong way label (BUG-029).

A removal can only restyle the element's other children, through `:first-child`, `:last-child`, `:only-child`
and `:nth-child` (subtypes 5 to 8 of the simple selector matcher `0x3ED9D0`) and the `+` and `~` combinators
(2 and 3 in `0x3EE1D0`). Cohtml 1.61 has no `:empty`, `:has` or `:not`. The fix follows each feature set from
its construction (the six calls of `0x3E8C10`) and reads every selector Cohtml adds to it (the call of
`0x3E8F80` at `0x3E9F41`, which every rule but a lone class, id, tag or `*` passes), keeping the compounds that
hold such a pseudo-class or sit right of `+` or `~` by what they need of an element: its tag, custom name, id,
classes and attributes. On a removal it looks up the element's feature set the way `0x37BC60` does and marks,
with Cohtml's own mark (`0x37B5A0`), only the children that could match one of those compounds. A compound with
nothing to tell elements apart, a feature set it did not see built, or a parent that is not a plain element
takes Cohtml's own invalidation. Twelve code regions are hashed and the seven calls checked first. The UI probe
counts both paths.

### Page fixes

A script added with View slot 61 (`AddInitialScript`) to the menu and HUD view, runs before every page's
own scripts. That view is the first one the game makes, and it also claims its own size, so a menu view
torn down and remade is recognised again instead of arriving as just another number. The ordinal is tried
before the size, which keeps a view whose settings could not be read from handing the identity to the
first car display. It keeps an animation frame counter and publishes its counts on
`window.__acevoUiFixes` for the UI probe.

- `SpatialNavigation.makeFocusable()` with no section runs once per frame, later calls in the frame fold
  into one scan on the next frame (BUG-025)
- `init` of `ks-page-vehiclesetup` is ignored on the same element while its own `Init` request is out
  (BUG-026)
- `onDevicesChanged` of `ks-page-settings-controls` runs a soft refresh at most once every 100 ms, the
  newest waiting one running when the time is up and replaced ones merging their settings

Every patch wraps the stock method and falls back to it.

### Resource work move

Cohtml hands its work to the game through `OnWorkAvailable`, and the game posts a job that calls
`Library::ExecuteWork` (slot 5), resources as type 0 with mode 0 and family -1. The game's frame thread runs
queued jobs while its UI frame end waits, and on some page loads it picked up the stylesheet parse. The
hook on slot 5 hands type 0 work called on the frame thread (the thread of EvoUi slot 8) to one mod thread
that makes the same call through the vtable, and runs any handed over work on the calling thread before
Cohtml's `StopWorkers` (slot 2) or `Uninitialize` (slot 3). Style and layout work (type 1) stays, the frame
waits for its result.

This part needs both checks to have passed, not only the UI engine one, because the frame thread it moves
work away from is learned from the frame end wrapper. On a build where the frame slots stood down it
installs nothing and says so.

That makes a new exe with the UI engine unchanged the interesting case, because the parts split three
ways. The restyle fix and the menu refresh fix patch the exe and stand down on its stamp. The resource
work move stands down too, not on a stamp of its own but because the frame end it needs is one of the
slots that just stood down. The style matching fix, the child removal fix, the narrowed stylesheet and the
page fixes all carry on, because they only ever needed the UI engine.

## Shared Cohtml hooks

`src/ui/cohtml_hooks.cpp` patches the exe's import of `Library::Initialize` and follows it to
`Library::CreateSystem` and `System::CreateView`, and wraps the game UI's frame post and end (EvoUi slots 7
and 8, vtable `0x3173D58`, checked against the exe's stamp). The responsive UI and the UI probe register
listeners from `DllMain`, in that order, and `InstallCohtmlHooks` installs once after both. Slot numbers
are in `include/acevo/ui/cohtml_hooks.h`.

The Cohtml side carries its own check, against the UI engine's stamp and image size, because Kunos ships
Coherent Gameface as its own binary and can update it without the exe changing, so the exe's stamp says
nothing about it. On a UI engine that is not the one the slots were read from, nothing that reaches a
Cohtml vtable is hooked at all, which takes the page fixes and the resource work move out together. The
log names the engine it found beside the one it wanted, or says plainly that it could not read the engine
at all, which is the other way that check refuses. Without it the slot numbers would be
used on a build they do not belong to, which is an indirect call through the wrong method rather than a
part quietly staying out.

The two checks are independent, so a game update that leaves Cohtml alone fails the exe half and passes
the UI engine half. The page fixes still go in, because a view only needs the engine. The resource work
move does not, because it only ever takes work off the frame thread and the frame end wrapper is where
that thread is learned, so `OnLibrary` asks `UiFrameEndHooked()` first and stays out with its own log line
rather than starting a thread that would wait forever for work that can never be handed to it.

## Limits

- Each part is written for game 0.9.1 (exe stamp `0x6A9EC72A`) and Cohtml 1.61.0.3 (stamp `0x675439C7`).
- A page's own script still builds the page in the frame it appears, and pages built over several frames
  still restyle much of the page each frame (BUG-028).
- The stylesheets are still read and parsed at every document load (BUG-027).
- `responsive_ui=0` turns every part off, there is no switch per part.
