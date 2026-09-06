---
name: DEC-010-no-ui-changes-ship
kind: decision
description: the mod carries nothing UI related after the lag hunt, no hook, no switch, no tool, on the owner's word, the findings stay in the research record
updated: 2026-09-06
links: [BUG-014-ui-pages-lag-on-open-switch-and-interaction, ui-lag-hunt-2026-09-06, TODO-011-ui-overhaul-through-injected-scripts]
date: 2026-09-06
area: ui
status: standing
superseded-by:
---

## Decision

After the UI lag hunt of 2026-09-06 the mod carries nothing UI related: no Cohtml hook, no
inspector switch, no `[ui]` section in the ini, no UI tools, no script injection. The owner
first asked for the overhaul to be tried (TODO-011), drove one round of it, felt no change and
found the controls list empty, and closed the topic: "Remove everything related to UI." The
hunt's findings and the engine's surface stay in `ui-lag-hunt-2026-09-06` so the topic is not
reopened from scratch, and the code of every instrument and patch is in the history between the
commits `added: Cohtml engine hooks` and `removed: every UI related piece of the mod`.

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

- The mod stays a performance and streaming mod. BUG-014 is won't fix on the owner's word and
  TODO-011 is dropped.
- If the topic ever comes back it starts from `ui-lag-hunt-2026-09-06`: the numbers, the
  engine's surface and the injection vehicle are known, and so are the two regressions the
  attempts produced.
