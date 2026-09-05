# ACEvoPerf

Performance mod for Assetto Corsa EVO (verified on 0.9.0+release.48). It ships as a `dstorage.dll`
placed next to the game executable. The game loads it as its DirectStorage runtime, the mod forwards
everything to the original runtime and fixes the engine's video memory budget on the way.

## What it does

- Caps the DirectStorage staging buffer the game asks for (1 GB, kept twice in VRAM). On a 6 GB
  GPU this alone stopped the crashes on car and track changes and at startup, and the menu icons
  that vanished.
- Gives the engine fixed streaming pools (1024 MB texture tile pool, 1433 MB mesh cap) instead of
  the sizes it computes mid transition, which on a 6 GB card were 633 MB in a race and 526 MB
  after a restart. Road and tyre textures stay sharp, also after restarting a session.
- Sets engine flags the release build otherwise ignores (pipeline state cache, intro skip, and any
  other bool, int32 or double flag you list in the ini).
- Caps swap chain latency at one frame for steadier pacing, and applies process tweaks (priority
  class, no Windows power throttling, 0.5 ms timer).
- Writes telemetry: a log, a per second timeline CSV and a per frame CSV, with a report script.

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

## Layout

- `include/acevo/` and `src/`: the DLL, headers and sources in matching folders per concern
  (`core`, `dstorage`, `engine`, `render`, `telemetry`, `overlay`), `src/dllmain.cpp` wires them
- `dist/`: the files that go into the game folder (ini, readme, built DLLs)
- `tools/`: Python tools for the package and the settings files, data files in `tools/data/`
- `third_party/directstorage/`: Microsoft's DirectStorage headers and runtime
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
