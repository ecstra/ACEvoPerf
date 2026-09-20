---
name: BUG-035-the-writing-on-the-ground-is-pixelated
kind: bug
description: the painted and chalked writing on the track surface shows blocky and low resolution up close while the tarmac under it is sharp, so whatever carries the writing is at a lower detail level than the surface it sits on
updated: 2026-09-20
links: [directstorage-streaming, BUG-007-blurry-road-and-textures, BUG-001-texture-low-mip-shown-before-streaming, BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles]
area: streaming
status: open
severity: bug
reported: 2026-09-20
parent:
---

## Symptom

Owner, 2026-09-20: "the writing on the ground is pixelated for some reason".

The screenshot is a close view of the track surface with the fan writing and markings on it. The
tarmac itself reads clean, with its grain visible. The white writing over it is blocky, the letter
edges break into square steps several pixels across, and the smaller marks lose their shape
entirely. The kerb and the run off in the same frame look normal. So this is not the whole surface
at a low level, it is the writing specifically.

## What is not known yet

Whether the writing is part of the road texture at a lower level than the rest of it, a separate
decal texture with its own streaming, or a decal rendered at a fixed resolution the game does not
scale. Each of those has a different owner and only the first two are anything the mod touches.

Worth separating from the known blur bugs before assuming a cause. BUG-007 was the whole road
settled soft and is fixed. BUG-001 was a surface carrying low mip until you approach it, closed as
the engine's behaviour on every card. Neither describes one layer of a surface being coarse while
the layer under it is sharp.

## What would settle it

A run with `streaming_trace=1` while parked next to marked tarmac, to see whether the writing has
its own texture and what level it is being given, and the same view at texture quality Ultra
against a lower setting, to see whether the game scales it at all.

## Notes

Reported from the owner's machine, the RTX 3060 Laptop, with the mod on. Not yet checked with the
mod off, so whether the mod is involved at all is open.
