---
name: settings-files
kind: doc
description: where the game keeps user data and how the binary settings files are structured
updated: 2026-09-11
links: [tools, BUG-006-distant-objects-pop-in, BUG-007-blurry-road-and-textures]
---

# Settings files

- User data lives in `%USERPROFILE%\Saved Games\ACE\`: `video.videosettings`,
  `audio.audiosettings`, the input files, `ui_storage.uistorage`, `GameModes\`, `Logs\`,
  `Replay\` and `mods\` (the official car mod folder since 0.8.1, server side
  `Saved Games\ACE-Server\mods`).
- The driver account and profile are `account.printabledriveraccount` and `ProfileData\<guid>\`
  (profile, personal settings, garage, saved cars, `custom_ffb_gain.txt`). Controls are the three
  root input files: `input_devices.inputdeviceconfiguration` (controller buttons),
  `input_keyboard.keyboardinputconfiguration` and `input_settings.inputsettings` (linearity and
  the other input values). `pipeline.library` is the shader pipeline cache, rebuilt at launch.
- A fresh account with the old graphics and controls is every file in the folder deleted except
  `video.videosettings` and the three input files. Done on 2026-09-11 when the owner moved to
  the Steam copy.
- Each settings file is a bare protobuf message, no wrapper. `video.videosettings` parses as
  `VideoSettings` (schema in `tools/data/proto_schema.txt`). `tools/acevo_settings.py` reads and edits
  them with the schema pulled from the exe at run time.

## Fields worth knowing in VideoSettings

- `graphics.textureQuality` (VeryLow to Ultra) and `graphics.texturePoolSize` (Low to Ultra, not
  exposed in the menu, unset means Low).
- `graphics.upscaling.mode` and `dlss_preset` or `fsr3_preset`. Ultra Quality renders 1443 by 812
  for 1080p, Quality 1280 by 720, DLAA native.
- `graphics.levelOfDetail.levelOfDetailCustomSettings`: `mainLodDistanceScale`,
  `mirrorLodDistanceScale`, `cubemapLodDistanceScale`, `shadowLodDistanceScale` and the
  `...OutDistanceScale` values, plus fixed car LODs for cubemap, mirror and shadow rendering.
- `graphics.anisotropicFiltering.customSettings`: main and low anisotropy, cubemap anisotropy,
  `mainAnisotropicMipBias`, `mainLowAnisotropicMipBias`.
- `graphics.clouds.cloudsCustomSettings`: `resolution`, `renderingTimeslicedOverFramesNumber`.
- `graphics.car_visibility`: `max_ahead_car`, `max_behind_car`.
- `display.frame_rate_limit`: gameplay, occluded and menu limits.

## Scene overrides

`GraphicsSettingsOverride` files (`.graphicsoverride`, referenced from `SceneGraphicsSettings`)
let a scene override AA, shadows, LOD, clouds, exposure and sun position. Per track tuning lives
there, inside the content package.
