---
name: BUG-007-blurry-road-and-textures
kind: bug
description: road and most surfaces settled at a soft detail level, fixed by the fixed pool size plus texture quality Ultra
updated: 2026-09-05
links: [settings-files, TODO-005-lap-two-experiments, BUG-001-texture-low-mip-shown-before-streaming, BUG-010-texture-pool-shrinks-on-race-load-and-restart]
status: fixed
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

- Root cause found on 2026-09-05 18:13: in a race the texture tile pool is only 633 MB (526 MB
  after a session restart) because the engine sizes it during the scene transition, see
  BUG-010. The menu gets 1117 MB from the same formula. Road and tyre textures are the most
  visible casualties of a small pool. `texture_tier0` was tested the same day and pins textures
  to the lowest tier, never enable.

## Fix

The fixed 1024 MB tile pool (BUG-010, commit d5197d5) plus the in game texture quality set to
Ultra by the owner. With a fixed pool the higher mips cannot exceed the budget.

## Verification

Owner after lap four of 2026-09-05: "roads look better. It was cuz I was in high. Nothing to
fix". Tile traffic per minute rose from 700 MB at High to about 1 to 1.7 GB at Ultra with VRAM
steady at 4.6 GB.

## Verification

Absent.
