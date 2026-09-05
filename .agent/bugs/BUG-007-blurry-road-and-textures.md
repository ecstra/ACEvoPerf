---
name: BUG-007-blurry-road-and-textures
kind: bug
description: road and most surfaces settle at a soft detail level
updated: 2026-09-05
links: [settings-files, TODO-005-lap-two-experiments, BUG-001-texture-low-mip-shown-before-streaming]
status: open
severity: bug
area: render
reported: 2026-09-05
parent:
---

## Problem

Owner wording: "Textures are streaming but road is not as detailed and still seems blurry. most of
the textures seem blurry. perhaps itz cuz im on high than max".

## Evidence

From the owner's `video.videosettings` of 2026-09-05 16:25:

- `textureQuality: TextureQuality_High` (Ultra exists). `texturePoolSize` is unset, which is the
  Low preset.
- Upscaling is DLSS Ultra Quality, rendering 1443 by 812 for a 1920 by 1080 output. Anything
  upscaled is softer than native.
- Anisotropic filtering High with a custom block of 8x main, 4x low, 1x cubemap and
  `mainAnisotropicMipBias` at 0.

## Fix

Absent. Sharper textures and a higher frame rate pull in opposite directions on this GPU, the
trade is the owner's, see TODO-005.

## Verification

Absent.
