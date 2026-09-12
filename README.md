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
  <img alt="Assetto Corsa EVO 0.9.0 and 0.9.1" src="https://img.shields.io/badge/Assetto_Corsa_EVO-0.9.0_and_0.9.1-E10600.svg">
  <br><br>
</p>

## Overview

A small performance mod for Assetto Corsa EVO. It is a `dstorage.dll` that sits next to the game exe, passes everything through to Microsoft's real DirectStorage runtime and fixes the game's video memory budget on the way. A few files in the game folder, no installer, delete them and you are back to stock. Nothing belonging to the game is replaced or edited.

It exists because a 6 GB card kept crashing the game on car and track changes, lost the icons in the vehicle hub and turned the road to mush after a restart. All of it came down to the same thing, which the mod corrects at start. Built on an RTX 3060 Laptop with 6 GB on Windows 11, and since reported working by people on RTX 2060, 3060 Ti, 3070 Ti, 4050 and 4060 cards, from 6 GB up. Developed against game versions 0.9.0 and 0.9.1.

The mod page, with the discussion and the reviews, is [on Overtake](https://www.overtake.gg/downloads/acevoperf.86467/). This repo carries the source, the changelog and the same zip as a release.

> [!NOTE]
> Every measured claim below comes from one machine, a 6 GB laptop. Other cards get their pool and buffer sizes picked automatically from their own memory, and users on 6 to 8 GB cards have reported it working, but the numbers quoted here are from that one laptop and yours will differ. If the game does not start or runs worse, uninstall (see below) and you are back to stock. If you report a problem, attach `acevo_perf.log` from the game folder, that file says exactly what the mod did.

## What it fixes

* **Crashes on car change, track change and at startup.**
* **Missing icons in the vehicle hub and the menus.**
* **Mushy road, tyre and ground textures,** worse after restarting a session.

It also adds NVIDIA Reflex to a game that ships none, skips the intro, and runs the game at above normal CPU and GPU priority. All of it is in the ini if you want it off.

## How it does it

**The crashes and the icons:** the game asks DirectStorage for a 1 GB staging buffer and the runtime keeps two of them in VRAM, so 2 GB of a 6 GB card are gone before anything loads. Swap a car or a track and the card runs out. In the menus the icon textures have nothing left to load into. The mod caps that buffer at 128 MB, which is still four of the biggest requests the game ever makes. Cards over 7 GB get 192 MB, over 11 GB 256 MB.

**The textures:** the engine sizes its texture pool from whatever VRAM is left mid transition, while the old scene is still resident, which on a 6 GB card was 633 MB in a race and 526 MB after a restart. The engine has two flags for exactly this, `force_canonical_pool_sizes` and `tile_pool_mb`, that the release build never reads from the command line. The mod finds their storage inside the running exe and writes them at start, so the pool is created once at 1024 MB (1536, 2048 or 3072 on bigger cards) and never shrinks. Texture quality needs to be on Ultra for this to show.

## What else it does

* **Brings a newer DirectStorage:** the game ships Microsoft's 1.2.3, the mod carries 1.3.0 and uses that instead. The one that matters is a fix from 1.2.4 for a race that could stop DirectStorage processing requests while the CPU is under heavy load, which is what a session load looks like here. Your game files are not touched, the newer runtime is simply loaded first. The log says which one is actually running, and `bundled_runtime=0` in the ini goes back to the game's.
* **Sizes itself to your card:** the tile pool and the staging buffer are picked from the render adapter's memory the moment the game creates its DXGI factory, before the renderer sizes its pools. A number in the ini overrides the pick.
* **Engine flags from the ini:** any bool, int32 or double gflag of the game can be set under `[flags]`. The release build ignores flags on the command line, so the mod locates the storage of each one inside the exe and writes it directly. `no_intro` is the only one on by default that is not part of a fix.
* **NVIDIA Reflex,** which the game does not have. It is not a frame rate limiter and caps nothing: it stops the CPU queueing frames further ahead of the GPU than it can use, so the input behind a frame is newer. NVIDIA only, and on an AMD or Intel adapter it never even loads the NVIDIA library, it checks the vendor of the adapter you actually render on and stays out of the way. Being honest about it: on the 6 GB laptop this was built on there was no frame rate change across four measured runs, because that machine is 97 percent GPU bound and sits pinned at its thermal limit, which leaves Reflex nothing to do. Latency, which is the point of it, was not measured.
* **Process tweaks:** above normal priority class, high GPU scheduling priority, Windows power throttling off for the game, 0.5 ms timer resolution.
* **A log that says what happened:** `acevo_perf.log` next to the exe lists everything applied, every DirectStorage queue and file, per queue streaming statistics and every frame slower than `hitch_ms` with the streaming activity around it. Two CSVs, one line per second and one per frame, are off by default and one line in the ini away.
* **Two GPU laptops:** the log says which adapter owns the monitor the window sits on, and warns when it is not the one rendering, because every frame is then copied across.
* **A mods folder:** files under `acevo_mods\` next to the exe replace files of the same path inside the 64 GB content package, or add new ones, without touching the package. Delete the folder to undo.

## Install

1. Close the game.
2. Download the zip from [Overtake](https://www.overtake.gg/downloads/acevoperf.86467/) or from the [latest release](https://github.com/ecstra/ACEvoPerf/releases/latest) here, they are the same file.
3. Open the game folder, the one with `AssettoCorsaEVO.exe` in it. In Steam that is right click the game, Manage, Browse local files.
4. Copy everything from the zip into that folder, the three files and the `acevo_perf` folder. Let Windows replace the existing `dstorage.dll`.
5. Start the game. `acevo_perf.log` appears next to the exe and lists what was applied.

After a game update that replaces `dstorage.dll`, do the same again. If an update breaks the mod, remove it until a new version is out.

## Uninstall

1. Close the game.
2. Delete `dstorage.dll`.
3. Rename `dstorage_orig.dll` to `dstorage.dll`.
4. Delete `acevo_perf.ini`, `acevo_perf.log` and the `acevo_perf` folder.

Or verify the game files in Steam, which puts the original `dstorage.dll` back, then delete the ini and the log.

## Settings

Everything is in `acevo_perf.ini`, every key is explained in the file.

## Build

`build.ps1` at the root, needs Visual Studio 2022 with the Windows SDK, output in `dist\`. `release.ps1` builds and zips the payload into `release\`. Set `ACEVO_GAME_DIR` to your game folder and `build.ps1 -Install` copies the build there. The Python tools under `tools\` (package inspection, settings files, telemetry reports) find the game the same way.

## Documentation

* [CHANGELOG.md](CHANGELOG.md): every version, what it fixed and how.
* [CONTRIBUTING.md](CONTRIBUTING.md): how to report a problem and what to include.

## Credits

`dstorage_orig.dll` and `acevo_perf\dstoragecore.dll` are Microsoft's DirectStorage 1.3.0 runtime from the NuGet package `Microsoft.Direct3D.DirectStorage`, redistributed under its license as that package allows (`third_party/directstorage`). The rest is by **ecstra**.

Licensed under MIT.
