---
name: BUG-020-track-textures-blur-when-many-cars-are-close
kind: bug
description: with ten cars close to the camera the texture streamer drops hundreds of track textures to their coarsest level for a second or more, because car textures outrank them in a tile pool that is already at its ceiling, and a field report of a crash after 40 minutes in a multiplayer lobby may be the same pressure
updated: 2026-09-13
links: [texture-streamer-flip-2026-09-13, DEC-017-streamer-reload-fix-refuses-the-drop, BUG-016-vram-overhead-grows-across-scene-loads, directstorage-streaming]
status: open
severity: medium
area: streaming
reported: 2026-09-13
parent:
---

## Problem

Owner wording, 2026-09-13: "The streaming was broken sometimes (back to blurry textures for a second
or two) when all 10 AI cars were close to one another." And on the multiplayer field report of
2026-09-06 (a 3060 Ti 8 GB desktop that froze and crashed after a 40 minute online Nordschleife
lobby, against 3 minutes before the mod): "I think thats cuz streaming is struggling when there are
other cars."

## Evidence

Session `logs/dyntrack-20260913`, a ten car instant race at the Nürburgring GP with
`streamer_reload_fix=1` and the streaming trace on.

- **The pool sat at its ceiling the whole race.** The engine's own counter read 15,300 to 15,360 of
  16,384 tiles used, and 15,360 is exactly the capacity minus the 1024 tile margin the load gate
  keeps. About 100 loads a kick were turned away for space. VRAM was 4,827 of 5,226 MB on the grid.
- **The blur is a mass drop.** Thirty seconds after the start, 12:54:40, the streamer dropped 257
  textures to level 0 in five seconds, 3,850 tiles, 240 of them track textures. Its demand list
  shrank from about 3,050 records to 2,140 in the same kicks. A reload burst of 88 then 229 MB/s
  followed, with the frame rate dipping to 69.
- **Displacement, not a lost snapshot.** Only 4 of the 257 were wanted back on the next kick and 22
  within five, 187 within twenty. Cars close to the camera carry the vehicle priority table, which
  outranks track distance tables, so the admission edge climbs over the track and the textures
  that fall out of admission are dropped at once, all the way to level 0.
- **The reload fix refused nothing in that burst.** It refused 0 drops between 875 and 881 s, and
  it never refuses a drop of a texture that is not admitted at all.

## Open

- Whether the blur is the same with `streamer_reload_fix=0`, not yet run.
- Whether the 40 minute crash is this. No logs arrived from that report. An 8 GB card gets a 1536 MB
  pool and 192 MB staging from `auto`, and a crash after a long session could equally be BUG-016's
  overhead or VRAM over the budget.

## Fix

Absent.

## Verification

Absent.
