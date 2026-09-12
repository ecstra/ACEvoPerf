---
name: BUG-006-distant-objects-pop-in
kind: bug
description: distant objects switch detail level visibly as the car approaches, reopened and traced to staticLevelOfDetail sitting unwritten at Low, which fixes all of it when turned on and halves the frame rate at every quality tier
updated: 2026-09-12
links: [settings-files, content-package, BUG-001-texture-low-mip-shown-before-streaming, TODO-005-lap-two-experiments]
status: wontfix
severity: nit
area: render
reported: 2026-09-05
parent:
---

## Problem

Owner wording: "Farther items pop-in and does not folow the same level we have?"

## Evidence

- Level of detail is the `graphics.levelOfDetail` block of `video.videosettings`. The custom mode
  exposes `mainLodDistanceScale`, `mirrorLodDistanceScale`, `cubemapLodDistanceScale`,
  `shadowLodDistanceScale` and the matching `...OutDistanceScale` values (schema in
  `tools/data/proto_schema.txt`).
- The owner's file on 2026-09-05 16:25 has the High preset with every scale at 1.0.

- Owner, 2026-09-06: "The grass looks like it pops in (not pop in jarringly, rather smoothly.
  like minecrafts render distance). I think its shadows and ambient occlusion that does this
  cuz trees are harder to miss and it has loaded, just the shadows and effects on trees come
  in later."
- The distances are written into the meshes of the content package (`MeshData` and
  `MeshLodData`, decoded with `tools/acevo_settings.py --message MeshData`):

  | mesh | levels | fade out |
  | --- | --- | --- |
  | `common_assets/meshes/vegetation/acc/low/grass_01/grass01_lod0.mesh` | density 0.5 from 6 m, 0.3 from 15 m, 0.2 after | `lodOut` 30 m, dithered over the last 10 m |
  | `.../grass_02/grass_02_nurburgring.mesh` | one switch at 40 m | `lodOut` 100 m, dithered over 30 m |
  | `.../big/alaska_pine/alaskacedar1_lod0.mesh` | `lodIn` 0, 100, 200 m, billboard from 500 m | `castShadows` on the first level only |
  | `nurburgring/content/ext_terrain/background_trees_1.mesh` | one level | `lodOut` 1000 m, `castFarShadow` |

  So the grass fade the owner sees is the dithered 20 to 30 m band by design, and a tree gets
  its shadow at the 100 m switch to the shadow casting model, which is the "shadows come in
  later".
- No engine flag reaches these distances (the gflag table has none for LOD, grass or shadow
  range). The video settings do: `graphics.levelOfDetail` Custom with `mainLodDistanceScale`
  and `mainLodOutDistanceScale` scale every `lodIn` and `lodOut`. The Nordschleife scene names
  no `graphicsSettingsOverride` file, so the override layer cannot inject a per track LOD
  override either, only edited mesh files.
- Lap 20, 2026-09-06 09:14: LOD Custom with switches 1.5 times and fade outs 2 times further,
  plus 16x anisotropic filtering with mip bias minus one. Owner: "it did something. but still
  pops in." Session average 79 fps against 85 before, 3.9 GB of tiles in the session.

- Owner, later on 2026-09-06, after watching online gameplay at maximum settings on an RTX
  5090: it does not happen there. "Even if it pops in, the grass entire color is changing (its
  dark green near, and light green in areas where it has not loaded). Thats why it looks wierd.
  same for trees." So the visible part is not the mesh fade alone, it is a colour change of the
  ground and the trees between "not loaded" and "loaded", which points at texture residency:
  a surface drawn from a low mip or without its detail layer until its tiles land, and on a
  32 GB card everything stays resident so nothing has to land. That makes it a streaming and
  VRAM symptom (BUG-001 is the same thing seen on the road), the mod's territory, and the tile
  pool size and eviction are the first suspects (the pool is 1024 MB on this card,
  `texturePoolSize` reads Low in the game log).

- Owner, an hour later: "I watched another video and it is clear that it happens on all
  cards, not just our mod. Seems like YT compression hides it well thats all."

## Fix

Won't fix, owner's call on 2026-09-06: the fade, the switch distances and the colour change
happen on every card, so they are the engine's design and not a fault of this machine or of
the mod. The LOD scale test (lap 20) moved the distances at a frame rate cost and is a quality
trade the game's own Custom level of detail setting already offers. The mesh numbers above
stay as reference.

## Reopened 2026-09-12, and the cause is named

The owner reopened this with three symptoms that turned out to be one:

1. Textures sharpen late, the spraypaint on the road arriving as the car reaches it.
2. Grass appears at a radius, "smooth popin like minecraft render distance".
3. Effects pop, shadows and sunlight on distant objects, and worst at night, where things the
   headlights are shining on jump from dark to lit as they come into range.

The settings file explains it. `video.videosettings` writes `levelOfDetail: High` and
`vehicleLevelOfDetail: High`, and never writes `staticLevelOfDetail` or
`experimentalStaticLevelOfDetail` at all. An unset protobuf field is zero, and zero for
`StaticLevelOfDetailQuality` is **Low**. So the level of detail for static world geometry, which is
the trees, the grass and the buildings, sat at the lowest value while the two settings beside it
were explicitly High. That is the same field for every player, not something about this machine.

Turning the subsystem on at High **fixed all three symptoms at once**, which is the useful part:
they were never three bugs, they were one. The owner's words: "I see that it works but it tanked my
fps by half."

## Why it cannot ship, measured

The cost is not the quality tier, it is the subsystem. A second lap with the subsystem on and the
quality at **Low** tanked exactly the same way, "FPS did not improve at all. Dropped to 39 at
times", and the game's own settings screen labels this option very heavy.

The cost is GPU, not anything the mod could reach. From the Low lap,
`logs/staticlod-low-1604`, 167 driving seconds:

| | slow seconds, under 45 fps | fast seconds, over 70 fps |
|---|---|---|
| seconds | 37 | 52 |
| process CPU, median | 24% | 25% |
| system CPU, median | 33% | 30% |
| tile streaming, median | 6.3 MB/s | 17.6 MB/s |

The CPU is the same whether the frame rate is 37 or 86, so the slow seconds are not CPU bound and
not streaming bound. It is geometry on a card already measured at 97 percent load and pinned at 86
degrees (the Reflex work). Rendering more distant geometry costs GPU time, the mod cannot
manufacture GPU headroom, and no engine flag reaches any of it: all 204 were read, and the only LOD
adjacent ones are destructive (`force_lowest_lod`, `no_gi`, `no_instances`,
`disable_shadow_depth_rendering`).

## Worth telling Kunos

Two things, independent of what the mod does:

- The settings screen never writes `staticLevelOfDetail`, so it sits at Low for everyone while the
  sliders next to it read High. Whether that is intended is their call, but it is invisible to the
  player.
- The option that fixes it is labelled experimental and costs half the frame rate on a 6 GB laptop
  card, so the honest reading is that the cheap path and the correct path are currently the same
  switch, with nothing in between.

## Verification

Absent for a fix, because there is no fix. The cause is verified twice, at High and at Low, with
the frame rate cost measured and the CPU ruled out.
