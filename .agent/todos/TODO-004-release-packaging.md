---
name: TODO-004-release-packaging
kind: todo
description: one zip and one script so anyone can install the mod without building
updated: 2026-09-05
links: [build-and-release]
status: open
by: owner
area: release
born: 2026-09-05
done:
---

## What

"I hope you are making sure that you are following the best practices (so that anyone can
install this as a mod later)."

## Done when

A `release.ps1` at the root runs `build.ps1` and zips `dist/` with a version stamp, the zip holds
`dstorage.dll`, `acevo_perf.ini`, `install.ps1`, `uninstall.ps1` and a short readme, the installer
finds the game through the Steam library as well as the known folders, and the version in the DLL
log line matches the tag. Binaries stay out of git.
