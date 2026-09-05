---
name: build-and-release
kind: doc
description: how to build, install, uninstall and (later) package the mod
updated: 2026-09-05
links: [TODO-004-release-packaging, proxy-architecture]
---

# Build and release

## Build

`build.ps1` at the root compiles every `.cpp` under `src/` (recursive) into `dist/dstorage.dll`
with the newest installed MSVC 2022 toolset and the newest Windows 10 SDK that has `d3d12.h`.
Headers are found through `/I include`. Flags: `/O2 /W4 /MT /std:c++17`, linked against
`kernel32.lib` and `user32.lib` only, exports from `src/exports.def`, version resource from
`src/version.rc` (compiled with `rc.exe`). A new source file needs no build script change, a new
folder neither.
The DirectStorage headers come from `third_party/directstorage` (Microsoft NuGet package
`Microsoft.Direct3D.DirectStorage` 1.2.3, MIT, license included). A clean build has zero errors
and one known warning (`C4244` inside the STL, from a `wchar_t` to `char` copy).

## Install

Drag and drop, no scripts (DEC-007). The zip holds `dstorage.dll` (the proxy), `dstorage_orig.dll`
(Microsoft's DirectStorage 1.2.3 runtime from `third_party/directstorage/bin/x64/`, byte identical
to the game's own file), `acevo_perf.ini` and `README.txt`. The user copies them into the game
folder and lets Windows replace `dstorage.dll`. Uninstall is the reverse, described in
`dist/README.txt`. If a game update ships a different DirectStorage version the proxy still loads
our 1.2.3 loader, which loads the game's `dstoragecore.dll`, the loader logs the versions.

## Release

`release.ps1` runs `build.ps1` (which also copies the runtime into `dist/` as `dstorage_orig.dll`),
then zips the four payload files into `release/ACEvoPerf-<FileVersion>.zip`. The version comes
from `src/version.rc`, keep it equal to `ACEVO_PERF_VERSION` in `include/acevo/common.h`. Built binaries
and the release folder stay out of git (`.gitignore`), the committed runtime DLL is the one
exception because the payload needs it and its license allows it.

## Gates

A change under `src/` or `include/` counts as verified when `build.ps1` is clean and a game launch
writes an `acevo_perf.log` that shows the new behaviour. A change to `tools/` counts as verified
when the tool runs against the real package or settings file.
