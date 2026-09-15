---
name: DEC-019-ui-lag-work-reopened
kind: decision
description: the UI lag is worked again as the one lane after the five deep dives of 2026-09-14, on the owner's pick, UI diagnostics may come back in developer builds off by default and UI fixes ship like any other fix once verified in game, superseding DEC-010
updated: 2026-09-14
links: [DEC-010-no-ui-changes-ship, BUG-014-ui-pages-lag-on-open-switch-and-interaction, ui-lag-deepdive-2026-09-14, TODO-027-the-ui-developer-build-and-one-session, ui-lag-hunt-2026-09-06]
date: 2026-09-14
area: ui
status: standing
superseded-by:
---

## Decision

The owner reopened the UI lag on 2026-09-13 for a deep dive, and after the five deep dives of 2026-09-14
picked it as the one lane to work: "Then pick one lane thats most promising and we'll do that. I pick ui
sinceits the most annoying." So the mod may carry UI work again. Diagnostics such as Cohtml hooks, view
timing and an initial script live in developer builds under `[developer]`, off by default. A UI fix goes
through the branch contract like any other fix and ships only when the owner has driven it and felt the
gain, with anything that changes page scripts delivered so a failure falls back to the stock code.

## Alternatives

- **Keep DEC-010.** Rejected by the owner, the UI is the issue that annoys most. The deep dive also found
  causes the hunt of 2026-09-06 had not, the controls page freeze as navigation scans in script, the pit
  menu updating the UI one frame in three, vehicle setup built twice, and the stylesheets parsed at every
  load, several of them reachable with small changes.
- **Diagnostics only.** Rejected, a diagnosis without a shippable fix repeats the hunt.
- **The TODO-011 overhaul.** Not reopened, its first item, building rows in slices, is shown not to touch the
  freeze, and its round broke the controls list.

## Consequences

- BUG-014 is open again, with BUG-024 to BUG-027 as its named parts and TODO-027 as the first step.
- The risk the hunt saw twice stays the rule to work under. Script patches fall back to the stock method,
  never run before their page is built, and stop when their page goes away, and the V8 profiler is not used.
- The mod is no longer only a performance and streaming mod, it also corrects the game's UI where the
  correction is verified.
