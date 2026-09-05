---
title: Road and most textures look blurry even though they finish streaming
status: open
severity: major
reported: 2026-09-05
updated: 2026-09-05
source: user, "Textures are streaming but road is not as detailed and still seems blurry. most of the textures seem blurry. perhaps itz cuz im on high than max"
---

## Symptom

Surfaces settle at a detail level that still looks soft, the road especially.

## Evidence

- The settings file read on 2026-09-05 12:42 (before the mod) had `textureQuality:
  TextureQuality_VeryLow` and no `texturePoolSize` value (which means the lowest preset). The
  user reports being on High now, to be re read after the lap.
- Upscaling is DLSS Ultra Quality, rendering 1443 by 812 and upscaling to 1920 by 1080. Anything
  upscaled is softer than native, DLAA would be the sharp reference.
- Anisotropic filtering is the High preset with a custom block of 8x main, 4x low, 1x cubemap,
  and `mainAnisotropicMipBias` unset (0).
- The `texture_tier0` engine flag ("Force Texture Tier 0") exists and is untested.

## Suspects

- Texture quality preset caps the highest mip that is ever streamed. Test: Ultra with the larger
  tile pool, watch `vram_used_mb`.
- Upscaler softness. Test: DLAA or native for one lap and compare.
- Negative mip bias not applied. Test: custom anisotropic block with `mainAnisotropicMipBias` at
  minus one.

## Fix

Not yet.
