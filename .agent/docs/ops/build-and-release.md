---
name: build-and-release
kind: doc
description: how to build, install, uninstall and (later) package the mod
updated: 2026-09-05
links: [TODO-004-release-packaging, proxy-architecture]
---

# Build and release

## Build

`build.ps1` at the root compiles `src/dllmain.cpp` into `dist/dstorage.dll` with the newest
installed MSVC 2022 toolset and the newest Windows 10 SDK that has `d3d12.h`. Flags: `/O2 /W4 /MT
/std:c++17`, linked against `kernel32.lib` and `user32.lib` only, exports from `src/exports.def`.
The DirectStorage headers come from `third_party/directstorage` (Microsoft NuGet package
`Microsoft.Direct3D.DirectStorage` 1.2.3, MIT, license included). A clean build has zero errors
and one known warning (`C4244` inside the STL, from a `wchar_t` to `char` copy).

## Install

`dist/install.ps1` refuses to run while the game is open, renames the Microsoft `dstorage.dll`
(checked by its version info) to `dstorage_orig.dll`, copies the proxy and, if none exists, the
default `acevo_perf.ini`. After a game update it recognises a fresh Microsoft DLL and refreshes
the original. `dist/uninstall.ps1` reverses it and removes the ini and log.

## Release

Not built yet, see TODO-004. Binaries stay out of git (`.gitignore`), releases carry them.

## Gates

A change to `src/dllmain.cpp` counts as verified when `build.ps1` is clean and a game launch
writes an `acevo_perf.log` that shows the new behaviour. A change to `tools/` counts as verified
when the tool runs against the real package or settings file.
