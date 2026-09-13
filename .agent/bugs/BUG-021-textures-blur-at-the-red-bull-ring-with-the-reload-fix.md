---
name: BUG-021-textures-blur-at-the-red-bull-ring-with-the-reload-fix
kind: bug
description: driving at the Red Bull Ring textures show blurry and then sharpen, and with the reload fix on the texture streamer turns away about twice as many loads for space as without it, likely because the tiles the fix keeps come out of the room loads get
updated: 2026-09-13
links: [DEC-017-streamer-reload-fix-refuses-the-drop, texture-streamer-flip-2026-09-13, TODO-019-tile-upload-dedupe-done-properly, BUG-020-track-textures-blur-when-many-cars-are-close]
status: open
severity: bug
area: streaming
reported: 2026-09-13
parent:
---

## Problem

Owner wording, 2026-09-13: "in red bull ring the textures are becoming blurry (though it loads
instantly after I see the blur)."

Seen on single laps of the Red Bull Ring, Time Attack in the Ferrari 296 GT3, with the shipped
settings, where `streamer_reload_fix` is on.

## Evidence

The texture streamer's `[streamer]` summary lines, turned into changes per pass. The streamer
passes about once a second, and a texture it wants sharper whose load does not fit the free pool
space is turned away and stays on its coarser mip until a later pass.

| Red Bull Ring laps | session | wanted sharper per pass | turned away per pass | drops refused per pass |
|---|---|---|---|---|
| single lap from the pits, fix on | `logs/memcreep-20260913/S-census-settled`, first visit | 106 | 83 | 3.0 |
| single lap from the pits, fix on | the same session, second visit | 113 | 81 | 3.0 |
| multiplayer event, fix on | `logs/frametime-20260913/P3-meshes-366` | 87 | 69 | 3.1 |
| ten hot laps, fix off | `logs/streamer-boot1-1124` | 54 | 38 | 0 |

With the fix on, twice as many loads are turned away for space, in two separate sessions, and the
tile pool sits near 15,000 of 16,384 tiles used in all of them.

The likely mechanism. The fix refuses a drop only while used plus pending tiles leave the engine's
1024 tile margin free, and the streamer's own load gate keeps that same margin back. With 15,100
tiles used the gate has about 260 tiles for loads, so every tile a refused drop keeps is a tile a
texture coming into view cannot get. Parked this costs nothing, the pool is full and the streamer
turns loads away on every pass with or without the fix. Driving, new textures need room every pass,
and the pinned finer mips hold it until their view moves away.

Not a pair. The fix off laps were ten consecutive hot laps with the streaming trace on, the fix on
laps were single laps from the pit lane, one of them a multiplayer event with other cars. The count
per pass also inflates with denials, a texture turned away asks again on the next pass.

## Fix

Absent. The next step is the same Red Bull Ring laps with `streamer_reload_fix=0` and `=1` in one
session order, the owner watching for the blur, then a refusal rule that leaves loads their room.
