---
name: BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring
kind: bug
description: at the Red Bull Ring textures show blurry for a moment after every camera cut of the pit menu showcase and then sharpen, with the reload fix off as much as on, the scenery because the 1024 MB texture pool is full and the car because the engine drops its livery at every cut even with room to spare, parked low priority
updated: 2026-09-13
links: [DEC-009-pool-and-staging-sizes-by-card, DEC-017-streamer-reload-fix-refuses-the-drop, texture-streamer-flip-2026-09-13, BUG-020-track-textures-blur-when-many-cars-are-close, BUG-010-texture-pool-shrinks-on-race-load-and-restart, TODO-019-tile-upload-dedupe-done-properly]
status: open
severity: nit
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

Absent.

## A bigger pool fixes the scenery and not the car, 2026-09-13

Session `logs/rbr-blur-20260913/C-pool-1536-fix-off`, the same pit menu wait with `tile_pool_mb=1536`
and nothing else changed. Owner wording: "it fixed it for most scenes except the car scene. so a
partial fix. Since the textures do load anyways, this is a low priority issue. Try another angle."

The streaming trace names the car's part. The Ferrari's livery, `skins\design_1\EXT_Skin_C` and
`EXT_Skin_D`, 340 tiles each at full detail, is dropped straight to its coarsest level by the
streamer's drop of textures not admitted at all the moment a shot of the track starts, and loaded
again when the camera comes back to the car, over one or two passes.

| livery `EXT_Skin_C` in the Red Bull Ring scene | pass | event |
|---|---|---|
| 21.3 s | 21 | loaded to full detail, gate space 18,630 tiles |
| 121.3 s | 127 | dropped to the coarsest level |
| 137.7 s and 138.7 s | 144, 145 | wanted full detail again on two passes |
| 154.1 s | 161 | dropped to the coarsest level |
| 170.5 s | 178 | wanted full detail again |
| 187.0 s | 195 | dropped to the coarsest level |

The gate had 11,000 to 20,000 tiles free through those cuts, so the drop is not the pool asking for
room. The engine throws away every texture of the shot it leaves, and the showcase comes back to the
car every 30 s or so. The livery never has a feedback reading (mip -1), so the car's detail comes
from the engine's own priority and not from what the shader saw.

Two angles for later, neither tried:

- **Keep what is not needed while nothing waits.** Refuse a drop of a texture not admitted at all
  while the previous pass turned no load away for space. With room it keeps the car across cuts. With
  the shipped 1024 MB pool at the Red Bull Ring a load is turned away on every pass, so it would never
  act there and changes nothing at the default, which is where the scenery blur lives.
- **A pool sized to the scene.** The scenery half needs room, and the fixed pool is 1024 MB so a 29 AI
  race keeps its headroom. The engine's own resizing measures the room while the previous scene is
  still in memory (BUG-010), so this means sizing it again after the unload.

Parked as low priority on the owner's word. It belongs with the streaming round of TODO-019, which
already covers the Red Bull Ring's texture traffic while driving.
