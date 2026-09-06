---
name: DEC-010-no-ui-changes-ship
kind: decision
description: the mod ships no change to the game's UI after the lag hunt, the Cohtml hook stays as a diagnostic with the inspector switch off, an overhaul is a separate decision of the owner
updated: 2026-09-06
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, ui-lag-hunt-2026-09-06, cohtml-ui-engine, TODO-011-ui-overhaul-through-injected-scripts]
date: 2026-09-06
area: ui
status: standing
superseded-by:
---

## Decision

After the UI lag hunt of 2026-09-06 the mod ships nothing that changes the game's UI. The
Cohtml hook in `src/ui/cohtml.cpp` stays because it costs nothing and gives the log the engine
version, the views and a working inspector switch (`[ui] inspector_port`, off by default). The
work split log, the layout thread, the V8 flags and the injected script patches were removed in
the same round. Whether to start the overhaul that would actually change what the owner feels
(TODO-011) is the owner's decision, taken on its cost, not something the mod slides into.

## Alternatives

- Ship the script patches that were tried (one visibility loop per element, one visibility
  sweep): rejected, they fix real defects of the UI's script but measured no change in the
  transitions and the owner felt none, and script injection into the game's UI is a risk the
  mod should only carry for a felt gain.
- Ship the layout thread: rejected, the render thread waits for the layout anyway, no change
  in any number, one more thread in the process.
- Keep going lever by lever: rejected by the owner after four hours, and the measurements say
  the cost is in the pages' scripts and the document reloads, where no engine lever reaches.

## Consequences

- The mod stays a performance and streaming mod, as `fix-real-bugs-only` in the machine memory
  and DEC-008 already framed it. BUG-014 stays open with the analysis, not fixed and not won't
  fix, until the owner decides on TODO-011.
- The next attempt starts from `ui-lag-hunt-2026-09-06` and `cohtml-ui-engine`, not from
  scratch: the numbers, the engine's surface and the injection vehicle are known.
