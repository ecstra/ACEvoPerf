---
name: free-roam-unlock-2026-09-06
kind: doc
description: the Free Roam mode hidden in 0.9.0 was found behind a password gate, switched on for an afternoon and driven, then dropped because the package stops at the Nürburgring complex, the mechanics and the content picture for whoever comes back to it
updated: 2026-09-06
links: [DEC-011-no-free-roam-mod, TODO-012-unlock-the-free-roam-mode, engine-flags, package-override-layer, content-package, moddability]
---

# Free Roam unlock

Done and undone on 2026-09-06 on 0.9.0+release.48. The mode was made to run through the mod
in seven rounds, the owner drove it, and the branch was deleted on the owner's word once the
package's limit was clear (DEC-011). Nothing of it is in the tree. This doc is the recipe and
the map of the content, so the topic restarts from here and not from zero.

## The gate

- The Drive menu's SOLO FREEROAM panel (`btnOpenWorld` in `uiresources\js\components.js`)
  already sends the `OpenWorld` request to the engine. The release build hides the panel with
  the style rule `body.release-build ks-page-main #btnOpenWorld { display: none !important }`
  in `uiresources\css\uicomponents.css`. The body class comes from the user agent's build
  tags, which the release exe leaves empty. A served copy of `uiresources\menu.html` with one
  later style rule shows the panel again.
- The request handler answers "Free Roam is not available in this build" unless the string
  flag `freeroaming_psw` ("unlocks the Free Roam gamemode and the Eifel scene when the
  expected password is provided") equals a 20 character literal built at startup from
  `.rdata` and compared by length and `memcmp`. The literal is plain text in the exe. The
  release build also reads `-freeroaming_psw` on the command line (see the memory
  `release-build-ignores-gflags-cli`), untested.
- String flags need care in the mod: a string flag keeps two `std::string` objects, a copy of
  the default built right before the registration (`lea rcx,[copy]; call; mov [rsp+28h],rax`)
  and the live one reached through a pointer variable (`mov rdx,[FLAGS_nox]`) that the exe
  fills during its static initialisation, after `DllMain`. The game reads the live one, and
  the scanner's lookback window must stop at the previous registration or it takes the
  previous flag's storage. Text above 15 characters must come from the game's own `malloc`
  (`ucrtbase.dll`) so the string's destructor can free it.
- Behind the gate the handler loads `content\data\free_roam.seasondefinition`, forces the
  track name to "Eifel", the nation to "DE" and the event to "Cruise" (three globals built at
  startup), sets the game mode type to FREEROAM (5) and looks the track and the layout up in
  `system\tracks.table` and `system\track_containers.table` (both `TableData` messages, read
  through the C runtime, not DirectStorage). The package has no Eifel track line and no
  Cruise layout, so the loader started with an empty scene path and the game died on
  "Trying to load a message with an empty path".

## What the package carries

- `nurburgring.scene` is the Eifel scene: its Track Info actor says track type Openworld,
  display name "eifel", coordinates 6.94 by 50.334. The complex is fully built, ring, GP
  track, boulevard and paddock, the tourist parking with open and closed gate variants, the
  villages of Adenau, Welcherath, Herschbroich, Quiddelbach and Drees as objects, gas stations,
  traffic lights and an open world spawns container (`spawns_ow.scene`).
- `system\freeroam_spawns.table` lists parking lots in Barweiler, Quiddelbach, Müllenbach,
  Welcherath, Drees and Wimbach. `system\gamemodes\free_roam.*.json` hold the A to B and time
  attack activities with their positions. `content\sfx\free_roam.bank` and
  `traffic_cars.bank` load with the mode, `system\traffic_settings.trafficsettings` and the
  traffic curves exist. The HUD has a free roam variant with a world map, a search and a
  points of interest list.
- The surroundings are 20 terrain chunks, `ext_terrain\terrain_near_lod0_1..16.mesh` and
  `terrain_far_1..4.mesh`, one low detail mesh per square kilometre or so, always drawn and
  carrying no physics. The Eifel terrain system (`content\terrains\Eifel\...`) is not shipped,
  only one ecotope texture under `content\resources\terrains\eifel`.

## What the package lacks

- Road physics beyond the complex: the physics containers cover the complex only, and no
  collision mesh names a public road. Physics meshes are plain static mesh actors with a
  `surface_id` and `physics_only`, so a container that points physics at the terrain chunks
  with the base container's unused "ROAD" surface makes them drivable, but rough: one
  surface for road and field, and bumps at every large triangle of the chunk.
- The free roam map: the HUD's `FreeroamMapManager` asks the engine's SlippyChart tile map
  for `content/tracks/eifel/eifel.terrainchart`, a `TerrainChartData` (GIS sources, a roads
  scene, styles, zoom limits, tile geometry). No such file, no tiles, no GIS data. A stub
  chart keeps the load from throwing and draws nothing.
- The road network: `FreeRoamRequestRoadNetwork`, the navigation, the car reset (respawn on
  the nearest road) and the points of interest all draw on it. The scene has no road or
  traffic actors.
- A dynamic track preset for the mode: the loader derives an empty name and logs
  "DynamicTrack preset not found", harmless.

## What made it run

Six files served by the package override layer under `acevo_mods`, generated from the
package's originals with the schema pulled from the exe: `uiresources\menu.html` with the
extra style rule, `system\tracks.table` with the Nürburgring line copied under the name
"Eifel", `system\track_containers.table` with a "Cruise" layout for it (the 24h base set
that joins ring and GP track, the boulevard and paddock, the tourist exit, the three park
gates open, `spawns_ow` and the physics container), `content\tracks\eifel\eifel.terrainchart`
as a stub, `content\tracks\nurburgring\containers\physics_2025.scene` (a registered container
no layout uses) filled with the 20 terrain chunks as ROAD physics, and the season file pointed
at the Nürburgring scene. Plus `freeroaming_psw` in the ini through string flag support in
the flag writer.

The working session is kept in `logs/freeroam1-20260906-1447/game_log.txt`: "ClientGameMode
assigned Eifel-Cruise id 19", 135 spawn points, the tourist parking and gate zones, the free
roam and traffic sound banks, the car at 4038, 570, 4827, the free roam HUD.

## Where it stops, and why it was dropped

Inside the complex everything is the real thing. Beyond it the car drives on low detail
terrain with rough collision, the world map stays empty, the pause menu's car reset does
nothing, no points of interest or activities appear, and falling off the terrain triggers the
engine's own "no load for 8 s, resetting to pits". All of it needs the Eifel scene Kunos has
not shipped in 0.9: the road network, the terrain physics and the chart tiles. A later build
that adds `content\tracks\eifel` is the moment to look again, with the table and physics
overrides left out.
