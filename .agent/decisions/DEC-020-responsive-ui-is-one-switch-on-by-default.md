---
name: DEC-020-responsive-ui-is-one-switch-on-by-default
kind: decision
description: every UI fix ships under one ini switch, responsive_ui, on by default, with the page fixes moved out of the developer probe, rather than a switch per fix or fixes that only run with the probe
updated: 2026-09-15
links: [responsive-ui, BUG-014-ui-pages-lag-on-open-switch-and-interaction, DEC-019-ui-lag-work-reopened, responsive-ui-rounds-2026-09-15]
date: 2026-09-15
area: ui
status: standing
superseded-by:
---

## Decision

After the menu refresh fix and the controls slider coalescing worked, the owner named the home: "give it a
proper home. call it "responsive_ui" or something. ON by default OFC." Every UI fix, the narrowed
stylesheet, the restyle fix, the menu refresh fix, the style matching fix, the page fixes and the resource
work move, installs under `[engine] responsive_ui`, and `responsive_ui=0` turns all of them off. The page
fixes left the developer probe's script for the responsive UI's own, and the probe only counts what they
do. The Cohtml hooks both need moved into one shared module.

## Alternatives

- **A switch per fix**, as the branch had with `ui_restyle_fix` and `ui_menu_refresh_fix`. Lost, the fixes
  are one felt result, every part already checks its game and UI engine build and stands down on its own,
  and the mod does not ship knobs that trade nothing.
- **Page fixes inside the probe.** Lost, they ran only with `ui_probe=1`, a diagnostic that stays off for
  players.

## Consequences

- A problem in one part is isolated by its log line and the build checks, not by the ini. Testing a part
  on its own takes a developer build.
- The probe and the responsive UI register with `cohtml_hooks` from `DllMain` and must do so before
  `InstallCohtmlHooks`.
