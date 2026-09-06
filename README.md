# ACEvoPerf

Performance mod for Assetto Corsa EVO (verified on 0.9.0+release.48). It ships as a `dstorage.dll`
placed next to the game executable. The game loads it as its DirectStorage runtime, the mod forwards
everything to the original runtime and fixes the engine's video memory budget on the way.

## What it fixes

- Crashes on car change, track change and at startup.
- Missing icons in the vehicle hub and the menus.
- Mushy road, tyre and ground textures, worse after restarting a session.

Verified on an RTX 3060 Laptop with 6 GB. Other cards get sizes picked for their memory at
start, untested so far.

## How it does it

The game loads `dstorage.dll` from its own folder as its DirectStorage runtime, and the mod is
that file. It forwards every call to Microsoft's real runtime, shipped next to it as
`dstorage_orig.dll`, and changes two numbers on the way through.

- The crashes and the icons: the game asks DirectStorage for a 1 GB staging buffer, the memory
  that file data lands in before it is copied into textures, and the runtime keeps two of them in
  video memory. So 2 GB of a 6 GB card are gone before the first texture loads. A car or track
  change holds the old scene while the new one comes in, runs the card out of memory and the
  game crashes, and in the menus the icon textures have nothing left to load into. The mod
  intercepts that one call and caps the buffer at 128 MB. The largest request the game ever
  issues is 32 MB, so four still fit in flight. Cards over 7 GB get 192 MB, over 11 GB 256 MB.
- The textures: the engine sizes its texture tile pool from the video memory free at the moment
  of a scene transition, while the previous scene is still resident. On a 6 GB card that gave
  633 MB in a race and 526 MB after a restart, too small for the sharp mip levels of the road and
  the tyres. The engine has two flags for exactly this, `force_canonical_pool_sizes` and
  `tile_pool_mb`, which the release build never reads from the command line. The mod finds their
  storage inside the running exe and writes them at start, so the pool is created once at
  1024 MB (1536, 2048 or 3072 on bigger cards, mesh cap 1433 MB) and never shrinks. Texture
  quality must be Ultra in the game settings for the full effect.

## What else it does

- Sets engine flags the release build otherwise ignores (pipeline state cache, intro skip, and any
  other bool, int32 or double flag you list in the ini).
- Applies process tweaks (priority class, no Windows power throttling, 0.5 ms timer) and logs
  what the game does to its swap chain.
- Writes `acevo_perf.log` with everything it applied. Two CSVs, one line per second and one per
  frame, are off by default (`timeline=1` and `frames=1` in the ini), with a report script.

## Install

Close the game, copy the three files from the release zip (`dstorage.dll`, `dstorage_orig.dll`,
`acevo_perf.ini`) into the game folder next to `AssettoCorsaEVO.exe`, let Windows replace the
existing `dstorage.dll`, start the game. `acevo_perf.log` next to the exe shows what was applied.

`dstorage_orig.dll` is Microsoft's DirectStorage 1.2.3 runtime, byte identical to the one the game
ships and redistributable under its license, so nothing needs renaming. To uninstall, delete
`dstorage.dll`, rename `dstorage_orig.dll` back to `dstorage.dll`, delete the ini and the logs.
After a game update that replaces `dstorage.dll`, copy the three files in again.

## Configure

`acevo_perf.ini` next to the exe. Every key is documented in the file itself.

## Build

`build.ps1` at the root, needs Visual Studio 2022 with the Windows SDK. Output goes to
`dist\dstorage.dll`. `release.ps1` builds and zips the drag and drop payload into `release\`.
For development set `ACEVO_GAME_DIR` to your game folder: `build.ps1 -Install` then copies the
build there, and the Python tools find the package and the exe through the same variable.

## Layout

- `include/acevo/` and `src/`: the DLL, headers and sources in matching folders per concern
  (`core`, `dstorage`, `engine`, `render`, `telemetry`, `overlay`), `src/dllmain.cpp` wires them
- `dist/`: the files that go into the game folder (ini, readme, built DLLs)
- `tools/`: Python tools for the package and the settings files, data files in `tools/data/`
- `third_party/directstorage/`: Microsoft's DirectStorage headers and runtime
- `assets/`: the icon for the mod listing and the script that renders it
- `logs/`: telemetry sessions copied from the game folder, gitignored
- `.agent/`: docs, bugs, todos, decisions

## Where to read more

- `.agent/INDEX.md`: the map of every document, bug, todo and decision in this repo
- `.agent/docs/research/moddability.md`: how the game is built and what can be changed
- `.agent/docs/foundation/proxy-architecture.md`: how the DLL works
- `.agent/docs/ops/telemetry.md`: what the log and CSV files contain
- `.agent/docs/ops/tools.md`: the Python tools for the package and the settings files
- `CLAUDE.md` and `CONTRIBUTING.md`: house rules for working in this repo

The DirectStorage headers in `third_party/directstorage` come from the Microsoft NuGet package
`Microsoft.Direct3D.DirectStorage` 1.2.3 (MIT, license included).
