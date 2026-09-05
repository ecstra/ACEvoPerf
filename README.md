# ACEvoPerf

Performance mod for Assetto Corsa EVO (verified on 0.9.0+release.48). It ships as a `dstorage.dll`
placed next to the game executable. The game loads it as its DirectStorage runtime, the mod forwards
everything to the original runtime and fixes the engine's video memory budget on the way.

## What it does

- Caps the DirectStorage staging buffer the game asks for. On a 6 GB GPU this grows the engine's
  texture tile pool and mesh pool from 400 MB to about 1100 MB each, and menu icons stop failing.
- Sets engine flags the release build otherwise ignores (pipeline state cache, intro skip, and any
  other bool, int32 or double flag you list in the ini).
- Applies process tweaks (priority class, no Windows power throttling, 0.5 ms timer).
- Writes telemetry: a log, a per second timeline CSV and a per frame CSV.

## Install

1. Close the game.
2. Run `dist\install.ps1` (pass `-GameDir` if the game is not in one of the known folders).
3. Start the game. `acevo_perf.log` next to the exe shows what was applied.

`dist\uninstall.ps1` puts the original DLL back. After a game update rerun the installer.

## Configure

`acevo_perf.ini` next to the exe. Every key is documented in the file itself.

## Build

`build.ps1` at the root, needs Visual Studio 2022 with the Windows SDK. Output goes to
`dist\dstorage.dll`.

## Where to read more

- `.agent/INDEX.md`: the map of every document, bug, todo and decision in this repo
- `.agent/docs/moddability.md`: how the game is built and what can be changed
- `.agent/docs/architecture.md`: how the DLL works
- `.agent/docs/telemetry.md`: what the log and CSV files contain
- `.agent/docs/tools.md`: the Python tools for the package and the settings files
- `CLAUDE.md`: house rules for working in this repo

The DirectStorage headers in `third_party/directstorage` come from the Microsoft NuGet package
`Microsoft.Direct3D.DirectStorage` 1.2.3 (MIT, license included).
