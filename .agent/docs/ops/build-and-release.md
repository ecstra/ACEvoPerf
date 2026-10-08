---
name: build-and-release
kind: doc
description: how to build, install, uninstall, package and publish the mod, the first releases cut on 2026-09-06, Overtake as the front door and GitHub as the mirror
updated: 2026-10-08
links: [public-docs, TODO-004-release-packaging, proxy-architecture, DEC-013-overtake-front-door-github-mirror, DEC-015-bundled-directstorage-core-loaded-first, directstorage-1-3-2026-09-12]
---

# Build and release

## Build

`build.ps1` at the root compiles every `.cpp` under `src/` (recursive) into `dist/dstorage.dll`
with the newest installed MSVC 2022 toolset and the newest Windows 10 SDK that has `d3d12.h`.
Headers are found through `/I include` and `/I third_party/directstorage`. Flags: `/O2 /W4 /WX /Brepro
/MT /EHsc /std:c++17` with `UNICODE`, `_UNICODE`, `WIN32_LEAN_AND_MEAN` and `NOMINMAX` defined, linked
against `kernel32.lib` and `user32.lib` only, exports from `src/exports.def`, version resource from
`src/version.rc` (compiled with `rc.exe`). A new source file needs no build script change, a new
folder neither, but two sources may not share a file name, since every object lands in `build\` under
its base name, and the script refuses the build when they do. The DLL carries only the PDB's file
name (`/PDBALTPATH:%_PDB%`), never the folder it was built in. Two builds of the same source have the
same code and differ only in the stamp and the PDB id, which the debug info makes new at each link.
The toolset is found through `vswhere.exe` and the `ProgramFiles(x86)` variable, nothing in the
script names a machine specific folder.

For development, `build.ps1 -Install` copies the fresh `dstorage.dll` into the folder named by the
`ACEVO_GAME_DIR` environment variable (refuses while the game runs, keeps an existing ini). Users
never run it, they install by drag and drop.

The DirectStorage headers and binaries come from `third_party/directstorage` (Microsoft NuGet
package `Microsoft.Direct3D.DirectStorage` 1.3.0, the binaries under Microsoft's own licence and the
headers under MIT, both included). The build copies `LICENSE.txt` and `NOTICES.txt` into `dist/` as
`acevo_directstorage_license.txt` and `acevo_directstorage_notices.txt`, and the zip carries them,
since the licence asks that its terms reach the people the binaries are passed to. A clean build has zero
errors and zero warnings, and `/WX` makes any warning fail the build.

## Install

Drag and drop, no scripts (DEC-007). The zip holds `dstorage.dll` (the proxy), `dstorage_orig.dll`
and `acevo_dstoragecore.dll` (Microsoft's DirectStorage 1.3.0, both from
`third_party/directstorage/bin/x64/`, the core renamed on the way into `dist/`), Microsoft's licence
and notices as `acevo_directstorage_license.txt` and `acevo_directstorage_notices.txt`, `acevo_perf.ini`
and `README.txt`. The user copies them into the game folder and lets Windows replace
`dstorage.dll`. Uninstalling is deleting the mod's files and letting Steam verify the game files,
which brings the game's own `dstorage.dll` back, as the readme and `dist/README.txt` say. Renaming
`dstorage_orig.dll` back, the uninstall up to 0.3.1, no longer gives the stock game, because that
file is Microsoft's 1.3.0 forwarder since DEC-015 and it runs on the game's 1.2.3 core.

`dstorage_orig.dll` is only a forwarder, the runtime is the core beside it, and the game claims the
name `dstoragecore.dll` at start-up, so the mod carries its core under a name nothing else asks for
and calls its entry points directly (DEC-015). No game file is replaced, so a game update shipping
its own DirectStorage changes nothing. A forwarder and a core have no version check between each
other, which is why the proxy reads `DStorageSDKVersion` off the module it actually loaded and
writes it to the log as `[runtime] DirectStorage 1.x.y in use`. That line is the gate for any
change here, because a mismatched pair works and says nothing, and the first attempt at this change
shipped looking correct while still running 1.2.3. How the forwarder finds its core, why no preload by the
proxy can win the name and how the raw SDK version number reads are in
[directstorage-1-3-2026-09-12](../research/directstorage-1-3-2026-09-12.md).

## Release

`release.ps1` runs `build.ps1` (which also copies Microsoft's two files into `dist/` as
`dstorage_orig.dll` and `acevo_dstoragecore.dll`, and its licence and notices), then zips the seven
payload files into
`release/ACEvoPerf-<FileVersion>.zip`. It refuses when the newest `CHANGELOG.md` heading is not this
version with a date, which an `(unreleased)` heading never is, or when any `[developer]` switch in
`dist/acevo_perf.ini` is not 0. The version comes
from `src/version.rc`, keep it equal to `ACEVO_PERF_VERSION` in `include/acevo/common.h`. Built binaries
and the release folder stay out of git (`.gitignore`), the two committed runtime DLLs are the
exception because the payload needs them and their license allows it.

Work does not land on main between releases. It collects on a long lived branch named for the version
being built, `0.4` today, and main moves only when that version is cut (DEC-021). The version branch
carries its own version from the day it opens, so `src/version.rc`, `include/acevo/common.h` and the
`CHANGELOG.md` heading are bumped on it rather than on main after the previous release.

To cut a release, swap `(unreleased)` in the version's `CHANGELOG.md` heading for the date on the version
branch, merge that branch into main, run `release.ps1` with the game closed, check the zip lists the five
files, and tag `v<version>` on the merge. Then open the next version's branch and bump the version files
on it in one commit, as [public-docs](public-docs.md) says. The zip name carries the four part file
version (`ACEvoPerf-0.3.0.0.zip` for 0.3.0). The newest commit on main is always the newest published zip,
which is what makes a clone of main mean something. The first release, 0.3.0, was cut on 2026-09-06 with the staging cap, the fixed pools,
the auto sizes, the flags, the overlay and the telemetry.

The icon for the mod listing is `assets/icon-512.png` (ACE over PERF, Bahnschrift Bold
Condensed on a black tile), with a 1024 px version next to it. `assets/icon.py` renders both
with Pillow and the Bahnschrift font that ships with Windows, and `assets/header.py` renders
`assets/header.png`, the banner at the top of the readme, the repo name in the pixel lettering
the owner's other repos use, with EVO outlined, from glyphs defined in the script.

## Publish

Two channels, the same zip, decided in DEC-013. In this order once the tag exists:

1. GitHub: `gh release create v<version> release/ACEvoPerf-<version>.0.zip --title
   "ACEvoPerf <version>"` with `--notes-file` pointing at notes written to
   [public-docs](public-docs.md), the Overtake link at the top, then what it fixes and adds,
   install, uninstall, the version's changelog section and the credits. `gh release edit`
   replaces the notes later.
2. Overtake: "Post an update" on the listing (`overtake.gg/downloads/acevoperf.86467`) with
   the same zip, the version number and a short update text. The listing's description
   holds the same content as the readme in plain paragraphs, the credits line for the
   Microsoft runtime included, and the icon is `assets/icon-512.png`. The listing's
   information link is the repo.
3. The repo's About link stays on the Overtake page, the readme's badge row and install step
   name both downloads as the same file.

Overtake filters uploads that contain game files, and up to 0.3.1 `dstorage_orig.dll` was byte
identical to the game's own, so a takedown without notice happened on 2026-09-06 and their support
restored the page the same evening. Since DEC-015 it is Microsoft's 1.3.0 forwarder and the game ships
1.2.3, so the zip no longer carries a game file, but a filter can still match on Microsoft's files. The mirror is what keeps the download reachable
in the meantime.

## Gates

A change under `src/` or `include/` counts as verified when `build.ps1` is clean and a game launch
writes an `acevo_perf.log` that shows the new behaviour. A change to `tools/` counts as verified
when the tool runs against the real package or settings file.
