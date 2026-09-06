---
name: ghost-car-records-but-never-shows
kind: memory
description: 0.9.0's ghostcar_enabled flag records every finished lap, saves the best as a .ghost file, loads it for the same car and layout and samples it every frame into a pose that nothing in the exe draws
updated: 2026-09-06
links: [ghost-car-2026-09-06, DEC-012-no-ghost-car-mod, engine-flags]
type: project
---

With `ghostcar_enabled` on, PlatformCore keeps a ghost data object, the replay recorder fills
it after every finished lap, the best lap is saved as
`Saved Games\ACE\GhostCar\ACEVO__<car>_<layout>.ghost`, the next session on the same car and
layout loads it, and the per frame update samples it into a car pose held inside PlatformCore.
No code reads that pose: the vehicle system has no ghost car type, nothing spawns a replay
driven car in a live session, and the UI has no ghost element.

It matters because the flag looks like a one line unlock and is not one. It makes the game
write files it never shows, so it must not ship (DEC-012), and a ghost would be engine work
from inside the proxy, not a flag.

Apply it by pointing anyone who asks at `ghost-car-2026-09-06`, and on a new game version by
running that doc's restart check before switching the flag on again.
