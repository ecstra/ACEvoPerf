---
name: DEC-011-no-free-roam-mod
kind: decision
description: the Free Roam unlock does not ship and its branch is gone, on the owner's word, because the 0.9 package stops at the Nürburgring complex, the recipe stays in the research record
updated: 2026-09-06
links: [free-roam-unlock-2026-09-06, TODO-012-unlock-the-free-roam-mode, engine-flags, package-override-layer]
date: 2026-09-06
area: engine-flags
status: standing
superseded-by:
---

## Decision

The Free Roam mode hidden in 0.9.0 was unlocked through the mod and driven on 2026-09-06, and
the same day the owner dropped it: "we will just remove free roam (just nuke this branch)".
The branch `feat/free-roam` was deleted, the overlay files were removed from the game folder,
the game folder went back to the 0.3.0 payload, and the string flag support the unlock needed
went with the branch. The mechanics, the content picture and the limits stay in
`free-roam-unlock-2026-09-06`.

## Alternatives

- Ship it as a second mod with the limits written down: rejected by the owner. Beyond the
  complex the roads are low detail and bumpy, the map is empty, the car reset and the
  activities do nothing, and none of that can be filled from the package.
- Keep the branch parked for the day Kunos ships the Eifel content: rejected, the owner asked
  for the branch to go. The research doc carries enough to rebuild it in an afternoon, and a
  package with `content\tracks\eifel` would make the table and physics overrides wrong anyway.
- Keep only the string flag support in the mod: not asked for, no shipped feature needs it,
  and the engine-flags doc says how it works if a string flag is ever wanted.

## Consequences

- The mod stays a performance and streaming mod with the 0.3.0 payload. TODO-012 is dropped.
- The finding itself is public knowledge the owner may write up: the mode exists in the
  release files behind a password and a hidden panel, and the Nürburgring scene is the Eifel
  open world's core.
- If the topic comes back it starts from the research doc, and the first check is whether the
  package now carries `content\tracks\eifel`.
