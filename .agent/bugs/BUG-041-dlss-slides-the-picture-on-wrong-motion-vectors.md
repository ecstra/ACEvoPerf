---
name: BUG-041-dlss-slides-the-picture-on-wrong-motion-vectors
kind: bug
description: with DLSS on the whole picture slides left and right now and then while the camera moves, worst in VR where each eye shifts on its own, because the game's motion vectors sometimes claim a third of a pixel of camera movement the rendered frame does not have, and DLSS follows them, while the jitter, its sign and the create flags are all correct
updated: 2026-10-08
links: [BUG-039-the-pso-cache-draws-some-materials-wrong, telemetry]
area: render
status: open
severity: bug
reported: 2026-10-08
parent:
---

## Problem

A player on the Overtake thread, 2026-10-01, Quest 3 over Virtual Desktop on an RTX 5090: with DLSS on, at any
preset and any quality up to DLAA at 150 percent, the image "appears to slightly flicker/jitter and shift
independently for each eye", stronger at lower render resolutions, gone without DLSS, and others confirm it.
The owner sees it on a flat screen too, with or without the mod: "the entirety of the texture is moving
left/right... road, fence, insides of the car... it happens sometimes and doesn't".

## Evidence

All on the owner's RTX 3060 Laptop at 1920 by 1080 with DLSS rendering at 1443 by 812, from temporary probes
on `fix/dlss-jitter` that were never committed, logs in `logs/dlss-jitter-20261008`.

- **What the game hands DLSS is clean.** The game calls NVIDIA's standard evaluate helper once per view
  (`0x1F8FA40` from `0x1F923A7`), each view with its own DLSS handle. The jitter is a 14 step Halton (2, 3)
  sequence in pixels, one step every frame with no skips. The create flags are 0x2B: HDR, low resolution
  motion vectors, depth inverted, sharpening, and motion vectors not jittered. The motion vector scale is 1.
- **The jitter sign is right.** 18 frame captures of DLSS's input and output read back from the GPU show the
  input image moving by the reported jitter every frame, in the reported direction, and the projection in
  memory carries it with the same sign. A run that flipped each axis in turn every 10 s measured no clearly
  steadier variant.
- **Still, it is perfect.** With the camera still, eight captures show motion vectors of exactly zero and an
  output that changes 50 times less than the input.
- **Moving, the motion vectors lie now and then.** On a patch of scenery, measured to about 0.05 px: in a
  few frames of every 18 the motion vectors jump by about 0.3 px against their neighbours (for example from
  0.12 to 0.29) while the rendered input shows no such jump. DLSS's output moves by exactly the motion
  vector's amount in those frames and comes back over the next ones. One frame rendered the same camera as
  the frame before, no movement at all, while its motion vectors claimed a full frame of it.
- **Where the camera lives.** The game keeps per pass blocks of view constants 0xA20 bytes apart with the
  view, the jittered projection, the camera position, the camera's movement since the last frame and a
  previous view relative to the current camera, which matches the previous frame's view exactly in normal
  frames. Snapshots taken at the evaluate call disagree between blocks every fourth frame, but they are taken
  while the next frame is already being built, so they do not yet name the frame the motion vectors go
  wrong in.

So the game's record of where the camera was last frame does not always match the camera it rendered last
frame. The game's own anti aliasing rejects history more readily than DLSS does, which fits the report that
it goes away without DLSS. In VR the game keeps right eye copies of the previous view and projection, which
fits each eye shifting on its own.

## Fix

Absent. A fix in the mod means finding the code that fills the previous view for the motion vector pass and
making it the view actually rendered the frame before, in the game's renderer, for both eyes.

## Verification

Absent.
