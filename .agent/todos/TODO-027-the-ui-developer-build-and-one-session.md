---
name: TODO-027-the-ui-developer-build-and-one-session
kind: todo
description: the UI lane, one developer build that times the engine side of the UI, toggles the pit menu flag and switches the controls page and vehicle setup fixes on alternate page entries, then one owner session of about 15 minutes that decides BUG-024 to BUG-026 and names the interaction cost
updated: 2026-09-14
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three, BUG-025-controls-page-scans-the-page-once-per-new-row, BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open, BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load, ui-lag-deepdive-2026-09-14, DEC-019-ui-lag-work-reopened]
status: open
by: owner
area: ui
born: 2026-09-14
done:
---

## What

Owner wording, 2026-09-14: "Then pick one lane thats most promising and we'll do that. I pick ui sinceits
the most annoying."

The lane starts with the decisive build of the UI deep dive
([ui-lag-deepdive-2026-09-14](../docs/research/ui-lag-deepdive-2026-09-14.md), the decisive build section),
off by default under `[developer]`.

1. The Cohtml hook chain with the 0.9.1 signatures (`OnWorkAvailable` takes a family, `ExecuteWork` is
   type, mode, family), `ExecuteWork` timed per thread and type, `View::Advance` timed per view.
2. EvoUi slots 7 and 8 wrapped with their thread ids and slot 8's wait per frame in the frames CSV.
3. EvoUi slot 24 wrapped, its flag arguments logged, and the menu flag forced on in 20 s blocks that
   alternate with off, with the rotation index per frame.
4. The V8 module compile wrapped for source length, cache offered or rejected, and milliseconds.
5. An initial script through View slot 61 logging navigation scans, focusables, focus, blur, tooltips and
   mouse events per view turn once a second, and switching the BUG-025 and BUG-026 fixes together on
   alternate page entries kept in `localStorage`. It stops itself when its page goes away.

Then one owner session with a numbered script, controls page clicks from the main menu, hover, scroll and
keyboard, the pit menu idle, vehicle setup hover and drag, controls from the pit lane, two laps, controls
from pause.

## Why

The deep dive split the UI lag into separate causes the 2026-09-06 hunt had not seen, with a small script
fix for the worst single freeze and an engine flag for the pit menu. One build with its own on and off
halves decides them in one session instead of a run per idea, and names the interaction cost that is still
unknown.

## Done when

The session is read and every pass or kill line of the build's section in the research doc is answered in
BUG-024, BUG-025, BUG-026 and BUG-014, and the fixes that passed are on a fix branch the owner has driven.
