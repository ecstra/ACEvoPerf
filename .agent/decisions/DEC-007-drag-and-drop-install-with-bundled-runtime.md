---
name: DEC-007-drag-and-drop-install-with-bundled-runtime
kind: decision
description: the zip bundles Microsoft's DirectStorage runtime as dstorage_orig.dll so installing is a plain copy
updated: 2026-09-05
links: [DEC-001-dstorage-proxy-as-loader, build-and-release, TODO-004-release-packaging]
date: 2026-09-05
area: release
status: standing
superseded-by:
---

## Decision

Ship `dstorage_orig.dll` inside the release zip, taken from the `Microsoft.Direct3D.DirectStorage`
1.2.3 NuGet package (`third_party/directstorage/bin/x64/dstorage.dll`, SHA-256 identical to the
file the game ships, listed in the package's `distributable_files.txt`). Installing is copying
three files into the game folder. `dist/install.ps1` and `dist/uninstall.ps1` are removed.

## Alternatives

- Installer script that renames the game's own DLL: correct but the owner ruled it out,
  "The installation must be as simple as drag and drop to your game folder".
- Proxy talking to `dstoragecore.dll` directly, no second DLL at all: needs the undocumented
  core entry points (`DStorageGetFactoryCore` and friends) and their exact contract, more
  reverse engineering for a two hundred kilobyte saving. Kept as a later option.

## Consequences

- Zero step install and a one line uninstall (delete, rename back).
- The repo carries one binary, the redistributable loader, with its license files next to it.
- If a future game build ships a newer DirectStorage, the proxy still forwards through the 1.2.3
  loader, which loads the game's newer `dstoragecore.dll`. The loader logs the versions, so a
  mismatch shows in `acevo_perf.log`.
