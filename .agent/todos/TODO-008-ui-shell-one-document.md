---
name: TODO-008-ui-shell-one-document
kind: todo
description: keep one UI document alive and swap page markup instead of reloading a document per page
updated: 2026-09-05
links: [BUG-008-ui-opens-slowly-with-loading-spinner, package-override-layer, TODO-007-package-override-layer]
status: open
by: agent
area: ui
born: 2026-09-05
done:
---

## What

Owner wording: "not just pause menu, rather all menus (settings page, in-game menu, pause menu,
menu in garage, menu when on track which is the same as pause, the screens in menu as well, the
menu on track but before i start the lap, etc...). basically all UI elements."

The UI bundle switches pages with `window.location.replace(page)` in `ksUI.goTo`, and every new
document re runs the 2.3 MB bundle (100 to 240 ms on the render thread, BUG-008). The shell
keeps the document and swaps only what is under `<body>`:

1. `acevo_shell.js` (served through the override layer) holds the body markup of every page it
   serves, a `switchTo(page)` that removes the current page's nodes, resets the body classes to
   the document scoped ones plus the new page's, inserts the new markup and recreates the
   notification panel, and a `prepare(page, path)` that sets the virtual path and the menu state
   the way a fresh document would find them.
2. The page files of the served pages (`hud.html`, `ingame.html`, `settings.html` first) become
   shell documents: same head plus the shell script, the page's own markup, and
   `ACEVO_SHELL.init("<page>")` at the end of the body. Whichever page the game enters first
   becomes the document, every later `goTo` to a served page is a swap.
3. `js/components.js` is patched: `goTo` calls the shell for served pages, the ten places that
   read `location.pathname` (origin, history, loading modal, `OnPageLoaded` to the engine) read
   the virtual path, the `reload` command re inserts the page, and the component lifecycle
   tracks the engine handlers a component registers while it initialises and clears them when
   the component leaves with its page (about 30 components never clear their own).
4. `engine._trigger` gets a per handler try catch so one stale closure cannot stop the others.

Phase one is the race set (HUD, pause and pitlane page, settings). Phase two adds the menu
pages (main, single player, multiplayer, vehicles, driver center, academy, gallery, paintshop,
part shop, simgrid, replay). `intro.html`, the car display and the driver label views stay as
they are.

## Why

Every pause, resume, settings open and pitlane return costs 150 to 500 ms of render thread
stalls and shows the spinner. It is the owner's first complaint about the UI and the reason the
override layer was built.

## Done when

Pause and resume in a race produce no `ACEVO_PAGE_START` line (no document load) and no frame
over 33 ms in `acevo_perf.log` at the switch, the pause menu, settings and pitlane pages look and
behave as before, and the same holds for the menu pages in phase two. Leaving to a page outside
the shell (intro, exit to desktop) still works.
