---
name: BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring
kind: bug
description: at the Red Bull Ring textures show blurry for a moment after every camera cut of the pit menu showcase and then sharpen, with the reload fix off as much as on, because the 1024 MB texture pool is full while 1.4 GB of video memory sits unused
updated: 2026-09-13
links: [DEC-009-pool-and-staging-sizes-by-card, DEC-017-streamer-reload-fix-refuses-the-drop, texture-streamer-flip-2026-09-13, BUG-020-track-textures-blur-when-many-cars-are-close, BUG-010-texture-pool-shrinks-on-race-load-and-restart]
status: open
severity: bug
area: streaming
reported: 2026-09-13
parent:
---

## Problem

Owner wording, 2026-09-13: "in red bull ring the textures are becoming blurry (though it loads
instantly after I see the blur)." And then: "The blur was in the game track menu (before I started
the lap, in the waiting pit lane area)... The scenery changes and then comes back to car. Each time
sceneray changes the starting is blurred. And then back to car its blurred (cuz its a new scene)."

The pit menu of a Red Bull Ring practice session shows the car, cuts to shots of the track and cuts
back to the car, over and over. After each cut the new view is blurry, most visibly the car's livery,
and sharpens a moment later. The owner's screenshot of a cut back to the car shows the rear bodywork
decals still coarse.

Reproduce: Red Bull Ring, Time Attack, Practice, Ferrari 296 GT3, stay in the pit menu a few minutes.

## Evidence

The texture streamer's `[streamer]` summary lines, changes per pass, the streamer passing about once
a second. A texture it wants sharper whose load does not fit the free pool space is turned away and
stays on its coarser mip until a later pass.

| Red Bull Ring pit menu | session | passes | wanted sharper per pass | turned away per pass | tile traffic |
|---|---|---|---|---|---|
| reload fix off, 5 minutes | `logs/rbr-blur-20260913/A-fix-off` | 319 | 73 | 46 | 29 MB/s |
| reload fix on, 3 minutes | `logs/memcreep-20260913/S-census-settled` | 184 | 81 | 57 | 20 MB/s |

- **It is not the reload fix.** The owner saw the blur at every cut with the fix off. With the fix
  on the streamer turns away about a quarter more loads per pass and carries a third less traffic,
  which is the fix's trade, and not a difference the owner could name.
- **The pool is full while video memory is not.** The tile pool sits at 14,900 to 15,360 of 16,384
  tiles in both sessions, which with the engine's 1024 tile margin leaves the loads almost nothing,
  and VRAM use is 3,755 MB of a 5,226 MB budget. A cut needs a whole new set of textures, and they
  wait until the textures of the previous shot are dropped and their tiles come back.
- **Driving does the same.** On single laps with the fix on the streamer turned away 69 to 83 loads
  per pass, against 38 on ten hot laps with the fix off earlier that day, not a pair.

The pool is 1024 MB because DEC-009 sizes it by the card, 1024 MB under 7 GB, so heavy scenes keep
their headroom. A 29 AI race reached 5,295 MB of the same 5,226 MB budget, so a bigger fixed pool
buys this scene room that a race does not have.

## Fix

Absent. The next step is the same pit menu wait with `tile_pool_mb=1536`, which answers whether pool
room is what the cut is waiting on.
