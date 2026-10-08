---
name: BUG-035-the-writing-on-the-ground-is-pixelated
kind: bug
description: the chalk writing on the Nordschleife's road shows blocky up close, partly the art, since graffiti_brunnchen4 is shown whole at only 1024 by 64, and partly the 1024 MB tile pool of a 6 GB card at Ultra, full there so the streamed decals are not admitted, neither of it the mod's, the pool side eased for bigger cards by the budget sizing of BUG-040
updated: 2026-10-08
links: [directstorage-streaming, BUG-007-blurry-road-and-textures, BUG-001-texture-low-mip-shown-before-streaming, BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles, BUG-040-big-cards-get-a-texture-pool-far-below-what-the-game-would-use, BUG-036-textures-resolve-in-visible-steps-and-a-mid-lap-restart-makes-it-worse]
area: streaming
status: wontfix
severity: bug
reported: 2026-09-20
parent:
---

## Problem

Owner, 2026-09-20: "the writing on the ground is pixelated for some reason".

The screenshot is a close view of the track surface with the fan writing and markings on it. The
tarmac itself reads clean, with its grain visible. The white writing over it is blocky, the letter
edges break into square steps several pixels across, and the smaller marks lose their shape
entirely. The kerb and the run off in the same frame look normal. So this is not the whole surface
at a low level, it is the writing specifically.

## Evidence

### What is not known yet

Whether the writing is part of the road texture at a lower level than the rest of it, a separate
decal texture with its own streaming, or a decal rendered at a fixed resolution the game does not
scale. Each of those has a different owner and only the first two are anything the mod touches.

Worth separating from the known blur bugs before assuming a cause. BUG-007 was the whole road
settled soft and is fixed. BUG-001 was a surface carrying low mip until you approach it, closed as
the engine's behaviour on every card. Neither describes one layer of a surface being coarse while
the layer under it is sharp.

### What would settle it

A run with `streaming_trace=1` while parked next to marked tarmac, to see whether the writing has
its own texture and what level it is being given, and the same view at texture quality Ultra
against a lower setting, to see whether the game scales it at all.

### Notes

Reported from the owner's machine, the RTX 3060 Laptop, with the mod on. Not yet checked with the
mod off, so whether the mod is involved at all is open.

### The art ships whole, 2026-10-08

The Nordschleife's road decals are their own textures in the package, read with `tools/texture_mips.py`:
`brunnchen_decal` 2048 by 256 with 12 levels, `brunnchen_decal2` and `3` 256 by 64 with 9,
`flugplatz_decal1` 512 by 512 with 10, `adenauer_forst_decal1` 256 by 256 with 9. Every one carries its
full chain, so the package holds the detail.

### The streamer never admits them, 2026-10-08

`logs/writing-steps-20261008`, the owner parked at the chalk writing on the Nordschleife for about twenty
seconds with `streaming_trace=1`, a 1024 MB pool on the RTX 3060 Laptop at Ultra. The owner's screenshot
shows the writing blocky beside a sharp road.

- The pool was full on every pass while parked, 16,384 tiles with the admission budget of 13,126 all
  taken, about 260 textures refused a place, and about 128 loads a pass turned away for space.
- `graffiti_brunnchen4` and its alpha map never loaded a tile in the whole session, but that is no
  refusal. The texture is 1024 by 64 with 11 mips in 12 tiles, which the streamer holds as a single
  level that is always resident, so it was at full size the whole time (corrected later the same day).
- `graffiti_general_2`, four streamed levels, was loaded whole at 45 s, dropped to its coarsest as not
  admitted at 70 s, loaded again at 202 s and dropped again at 204 s.
- The reload fix held about 170 MB of drops the engine wanted to make while parked, 76 refused, but all
  of it was in view with fresh feedback, the arid ground normal map and the guardrails, so dropping it
  would only have reloaded it a pass later. It does not stand between the writing and a place.

So the writing is blocky for two reasons. `graffiti_brunnchen4` is shown whole and is only 1024 by 64
pixels, which is blocky up close at that size on the road, so it is the art. The streamed decals like
`graffiti_general_2` are ranked low in a pool too small for everything in view at the Nordschleife at
Ultra on a 6 GB card. Nothing of the mod's keeps either of them out.

## Fix

Not fixed for the 6 GB card, on the owner's call of 2026-10-08. Writing drawn from a texture as small as
`graffiti_brunnchen4` stays blocky on every card. For the streamed decals, the 1024 MB pool already leaves about
600 MB of that card's budget spare, so there is no memory to give, and changing how the engine ranks
textures for admission means reworking its streamer from the inside. Texture quality one step down eases
the pressure. Cards of 8 GB and more get two to six times the pool since the budget sizing of BUG-040
(DEC-025), which should leave room for the writing there. A parked run at `tile_pool_mb=1536` on the 6 GB
card would confirm the reading directly and was not run.

## Verification

Absent.
