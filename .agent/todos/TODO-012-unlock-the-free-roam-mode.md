---
name: TODO-012-unlock-the-free-roam-mode
kind: todo
description: the engine ships a Free Roam game mode behind a password switch, unlock it from the mod and get the owner driving the roads around the Nordschleife
updated: 2026-09-06
links: [free-roam-unlock-2026-09-06, DEC-011-no-free-roam-mod, engine-flags, package-override-layer, moddability, TODO-009-overlay-serves-copies-so-loose-files-stay-editable]
status: dropped
by: owner
area: engine-flags
born: 2026-09-06
done: 2026-09-06
---

## What

Owner wording, 2026-09-06: "The game does not have freeroam yet. So if its in the files,
enabling this and letting me do freeroam is the biggest win. And if it works we can make this
another mod. Its not in the UI either, so we must enable it there as well."

## Where it stands

Found in the exe and the package on 2026-09-06:

- The menu's SOLO FREEROAM panel (`btnOpenWorld`, tooltip "Drive in the wonderful roads
  around Nordschleife") already sends the `OpenWorld` request to the engine.
- The engine answers "Free Roam is not available in this build" unless the string flag
  `freeroaming_psw` ("unlocks the Free Roam gamemode and the Eifel scene when the expected
  password is provided") equals a 20 character literal built at startup and compared with
  `memcmp`. The literal sits in `.rdata`, the check is the function the request handler
  calls first.
- The mod could not write string flags before this todo, so the first step is string flag
  support in `src/engine/flags.cpp`, written in the late pass because the exe constructs the
  flag's `std::string` after `DllMain`.
- `content\data\free_roam.seasondefinition` points at `content\tracks\eifel\eifel.scene`,
  which the package does not carry. The Nürburgring package does carry the Nordschleife
  cruise layout, the open world spawns container, the Adenau and village objects, the gas
  stations and the traffic data, and `system\freeroam_spawns.table` lists parking lots in
  Barweiler, Quiddelbach, Müllenbach, Welcherath, Drees and Wimbach. If the Eifel scene is
  what blocks the session, the override layer can serve a season definition that points at
  the Nürburgring scene with the free roam containers.

## Done when

The owner drives in the Free Roam mode started from the game's own menu with the mod
installed, the switch documented in the ini and the changelog, and the record says which
content the session uses.

Dropped 2026-09-06 after the drive. It ran, seven rounds on a branch the owner then had
deleted (DEC-011): the package stops at the Nürburgring complex, beyond it the roads are low
detail and rough, the map is empty and the car reset does nothing, and none of it can be
filled from the files. The recipe and the content picture are in `free-roam-unlock-2026-09-06`.
