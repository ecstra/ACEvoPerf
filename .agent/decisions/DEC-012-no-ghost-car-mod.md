---
name: DEC-012-no-ghost-car-mod
kind: decision
description: the ghost car flag does not ship and its branch is gone, on the owner's word, because 0.9.0 records and loads a ghost but has nothing that shows it, the findings stay in the research record
updated: 2026-09-06
links: [ghost-car-2026-09-06, DEC-011-no-free-roam-mod, engine-flags]
date: 2026-09-06
area: engine-flags
status: standing
superseded-by:
---

## Decision

The owner asked for ghost cars on 2026-09-06 ("ace does not have ghosts. enable them").
The flag `ghostcar_enabled` went into the ini on `feat/ghost-car`, the owner drove two
sessions, the game recorded, saved and loaded a ghost and showed no car, and the disassembly
explained why: the sampled ghost pose is written into PlatformCore every frame and read by
nothing. The owner's word was "nuke it". The branch was deleted unpushed, the game folder is
back on the 0.3.1 ini, the ghost file and its folder were removed from the save folder. The
findings are in `ghost-car-2026-09-06`.

## Alternatives

- Ship the flag on as a recorder: rejected. It writes a ghost file after every finished lap
  and shows nothing for it, which is a cost with no gain and a support question waiting to
  happen.
- Build the presentation in the proxy: not started. It means spawning a replay source vehicle
  through the vehicle system's internal interface and driving it from the pose every frame,
  which is engine work of a different size than anything the mod does, and it would break on
  every game update.
- Wait for a hotlap event or another mode to show it: rejected by the evidence, the consumer
  is absent from the exe, not gated by the mode.

## Consequences

- The mod stays a performance and streaming mod. No flag, no code and no ini line from the
  experiment is in the tree.
- Like the free roam mode (DEC-011), the finding is public knowledge the owner may write up:
  the ghost car exists in the release files up to the point of being drawn.
- If the topic comes back it starts from the research doc's restart check on the new build.
