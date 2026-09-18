<p align="center">
  <img
    width="760"
    alt="ACEvoPerf"
    src="assets/header.png" />
</p>

<p align="center">
  <br>
  <a href="https://www.overtake.gg/downloads/acevoperf.86467/"><img alt="Mod page on Overtake" src="https://img.shields.io/badge/overtake.gg-mod_page-E10600.svg"></a>
  <a href="https://github.com/ecstra/ACEvoPerf/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/ecstra/ACEvoPerf?label=release&color=brightgreen"></a>
  <a href="https://github.com/ecstra/ACEvoPerf/releases"><img alt="Downloads" src="https://img.shields.io/github/downloads/ecstra/ACEvoPerf/total?color=blue"></a>
  <a href="LICENSE"><img alt="MIT licence" src="https://img.shields.io/badge/licence-MIT-yellow.svg"></a>
  <a href="https://github.com/ecstra/ACEvoPerf/releases/latest"><img alt="Windows 11" src="https://img.shields.io/badge/windows-11-0078D6.svg"></a>
  <img alt="Assetto Corsa EVO 0.9+" src="https://img.shields.io/badge/Assetto_Corsa_EVO-0.9%2B-E10600.svg">
  <br><br>
</p>

## Overview

A performance mod for Assetto Corsa EVO 0.9 and newer. You copy a few files into the game folder and that's it, no installer. The only game file it replaces is `dstorage.dll`.

Made on an RTX 3060 Laptop GPU with 6 GB. Players have also reported it working on RTX 2060, 3060 Ti, 3070 Ti, 4050 and 4060 cards.

## What it fixes

- Crashes at startup and on car or track changes
- Missing icons in the vehicle hub and menus
- Mushy road, tyre and ground textures
- Blurry cars in races with AI
- Blurry trackside big screens
- Laggy menus
- Uneven frame pacing while driving
- Unnecessary texture streaming
- A memory leak growing with every track and menu you load

## What it adds

- NVIDIA Reflex
- Higher CPU and GPU priority
- A newer DirectStorage
- The game's shader cache, for fewer stutters

What changed in each version is in the [changelog](CHANGELOG.md).

## Install

1. Close the game.
2. Download the zip from [Overtake](https://www.overtake.gg/downloads/acevoperf.86467/) or the [latest release](https://github.com/ecstra/ACEvoPerf/releases/latest). Both are the same file.
3. Open the game folder. In Steam, right click the game, then **Manage** and **Browse local files**.
4. Copy all the files from the zip into that folder, next to `AssettoCorsaEVO.exe`.
5. When Windows asks, replace the file.
6. Start the game.

For the sharpest textures, set texture quality to Ultra in the game's graphics settings.

After a game update, copy the files in again.

## Uninstall

1. Close the game.
2. In the game folder, delete `dstorage.dll`, `dstorage_orig.dll` and everything whose name starts with `acevo_`.
3. In Steam, right click the game, then **Properties**, **Installed Files** and **Verify integrity of game files**. This puts the game's own `dstorage.dll` back.

## Settings

Everything is in `acevo_perf.ini` in the game folder, one line per setting with a short note. Any fix can be turned off there. Changes apply the next time you start the game.

## Problems

If the game won't start or runs worse with the mod, uninstall it. To report a problem, post on the [Overtake page](https://www.overtake.gg/downloads/acevoperf.86467/) or [open an issue](https://github.com/ecstra/ACEvoPerf/issues), and attach `acevo_perf.log` from the game folder.

## How it works

The game loads Microsoft's DirectStorage from `dstorage.dll` in its folder to read its files. The mod's `dstorage.dll` takes that spot, passes everything on to the real DirectStorage and fixes these problems while the game runs.

- **Crashes and missing icons:** the game sets aside about 2 GB of video memory just for loading, which leaves a 6 GB card short. The mod sets aside only what loading needs.
- **Mushy textures:** the game decides how much memory textures get while the previous track is still loaded, so they get too little. The mod sets it once, sized for your card.
- **Blurry cars in races:** once texture memory is full, the game ranks textures by the car that needs them least and loads new detail only when all of it fits. The mod ranks them by the car that needs them most and loads what fits.
- **Big screens:** the screens fall back to very low detail copies of their video. The mod hides those copies, so the sharp one always shows.
- **Menus:** the menus redo far more work than they need to on every hover and page change, and in a session they only update every third frame. The mod cuts the extra work and updates them every frame.
- **Frame pacing:** the game splits its screen updates between the HUD and the car's displays, so some frames carry far more work than others. The mod gives the HUD every frame and lets the car displays take turns, which evens the frame times out.
- **Texture streaming:** the game keeps dropping and reloading the same textures, even with the car parked. The mod stops that.
- **Memory leak:** the game never lets go of a track or menu once you leave it, so every load adds to its memory until you quit. The mod lets go of each one when the next one starts.

## Build

1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) with the **Desktop development with C++** workload.
2. Clone the repo.
   ```powershell
   git clone https://github.com/ecstra/ACEvoPerf.git
   cd ACEvoPerf
   ```
3. Build it.
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\build.ps1
   ```
4. The mod is now in `dist\`, the same files as the release zip. Install them the same way.

To build the zip itself, run `release.ps1` the same way. It goes into `release\`.

To copy each build straight into the game, set `ACEVO_GAME_DIR` to the game folder and add `-Install`. An ini already in the game folder is kept.

```powershell
$env:ACEVO_GAME_DIR = "<game folder>"
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Install
```

## Credits

`dstorage_orig.dll` and `acevo_dstoragecore.dll` are Microsoft's DirectStorage 1.3.0, included under Microsoft's license. Everything else is by **ecstra**, under the [MIT licence](LICENSE).
