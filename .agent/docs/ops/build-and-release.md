---
name: build-and-release
kind: doc
description: how to build, install, uninstall, package and publish the mod, the first releases cut on 2026-09-06, Overtake as the front door and GitHub as the mirror
updated: 2026-09-12
links: [TODO-004-release-packaging, proxy-architecture, DEC-013-overtake-front-door-github-mirror, DEC-015-bundled-directstorage-core-loaded-first]
---

# Build and release

## Build

`build.ps1` at the root compiles every `.cpp` under `src/` (recursive) into `dist/dstorage.dll`
with the newest installed MSVC 2022 toolset and the newest Windows 10 SDK that has `d3d12.h`.
Headers are found through `/I include`. Flags: `/O2 /W4 /MT /std:c++17`, linked against
`kernel32.lib` and `user32.lib` only, exports from `src/exports.def`, version resource from
`src/version.rc` (compiled with `rc.exe`). A new source file needs no build script change, a new
folder neither. The toolset is found through `vswhere.exe` and the `ProgramFiles(x86)` variable,
nothing in the script names a machine specific folder.

For development, `build.ps1 -Install` copies the fresh `dstorage.dll` into the folder named by the
`ACEVO_GAME_DIR` environment variable (refuses while the game runs, keeps an existing ini). Users
never run it, they install by drag and drop.

The DirectStorage headers and binaries come from `third_party/directstorage` (Microsoft NuGet
package `Microsoft.Direct3D.DirectStorage` 1.3.0, MIT, license included). A clean build has zero
errors and one known warning (`C4244` inside the STL, from a `wchar_t` to `char` copy).

## Install

Drag and drop, no scripts (DEC-007). The zip holds `dstorage.dll` (the proxy), `dstorage_orig.dll`
and `acevo_dstoragecore.dll` (Microsoft's DirectStorage 1.3.0, both from
`third_party/directstorage/bin/x64/`, the core renamed on the way into `dist/`), `acevo_perf.ini`
and `README.txt`. The user copies them into the game folder and lets Windows replace
`dstorage.dll`. Uninstall is the reverse, described in `dist/README.txt`.

`dstorage_orig.dll` is only a forwarder, the runtime is the core beside it, and the game claims the
name `dstoragecore.dll` at start-up, so the mod carries its core under a name nothing else asks for
and calls its entry points directly (DEC-015). No game file is replaced, so a game update shipping
its own DirectStorage changes nothing. A forwarder and a core have no version check between each
other, which is why the proxy reads `DStorageSDKVersion` off the module it actually loaded and
writes it to the log as `[runtime] DirectStorage 1.x.y in use`. That line is the gate for any
change here: a mismatched pair works and says nothing, and the first attempt at this change shipped
looking correct while still running 1.2.3.

## Release

`release.ps1` runs `build.ps1` (which also copies Microsoft's two files into `dist/` as
`dstorage_orig.dll` and `acevo_dstoragecore.dll`), then zips the five payload files into
`release/ACEvoPerf-<FileVersion>.zip`. The version comes
from `src/version.rc`, keep it equal to `ACEVO_PERF_VERSION` in `include/acevo/common.h`. Built binaries
and the release folder stay out of git (`.gitignore`), the two committed runtime DLLs are the
exception because the payload needs them and their license allows it.

Cutting a release: date the `Unreleased` section of `CHANGELOG.md` as the version, run
`release.ps1` with the game closed, check the zip lists the five files, commit and tag
`v<version>`. The zip name carries the four part file version (`ACEvoPerf-0.3.0.0.zip` for
0.3.0). The first release, 0.3.0, was cut on 2026-09-06 with the staging cap, the fixed pools,
the auto sizes, the flags, the overlay and the telemetry.

The icon for the mod listing is `assets/icon-512.png` (ACE over PERF, Bahnschrift Bold
Condensed on a black tile), with a 1024 px version next to it. `assets/icon.py` renders both
with Pillow and the Bahnschrift font that ships with Windows, and `assets/header.py` renders
`assets/header.png`, the banner at the top of the readme, the repo name in the pixel lettering
the owner's other repos use, with EVO outlined, from glyphs defined in the script.

## Publish

Two channels, the same zip, decided in DEC-013. In this order once the tag exists:

1. GitHub: `gh release create v<version> release/ACEvoPerf-<version>.0.zip --title
   "ACEvoPerf <version>"` with `--notes-file` pointing at notes written in the readme's voice
   (what it fixes, the important block, install, uninstall, the version's changes, credits,
   the Overtake link at the top). `gh release edit` replaces the notes later.
2. Overtake: "Post an update" on the listing (`overtake.gg/downloads/acevoperf.86467`) with
   the same zip, the version number and a short update text. The listing's description
   holds the same content as the readme in plain paragraphs, the credits line for the
   Microsoft runtime included, and the icon is `assets/icon-512.png`. The listing's
   information link is the repo.
3. The repo's About link stays on the Overtake page, the readme's badge row and install step
   name both downloads as the same file.

Overtake filters uploads that contain game files, and `dstorage_orig.dll` is byte identical
to the game's own, so a takedown without notice can happen. It did on 2026-09-06 and their
support restored the page the same evening. The mirror is what keeps the download reachable
in the meantime.

## Gates

A change under `src/` or `include/` counts as verified when `build.ps1` is clean and a game launch
writes an `acevo_perf.log` that shows the new behaviour. A change to `tools/` counts as verified
when the tool runs against the real package or settings file.
