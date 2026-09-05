---
title: Package the mod so anyone can install it without building
status: open
created: 2026-09-05
updated: 2026-09-05
source: user, "I hope you are making sure that you are following the best practices (so that anyone can install this as a mod later)"
---

## Done when

A zip built by one script contains `dstorage.dll`, `acevo_perf.ini`, `install.ps1`,
`uninstall.ps1` and a short readme, and a fresh machine can install it by running one script.

## Steps

1. `release.ps1` at the root that runs `build.ps1` and zips `dist/` with a version stamp.
   Check: zip opens and contains the five files.
2. The installer detects the game folder from the Steam library as well as the known paths.
   Check: works when the game is not at any hardcoded location.
3. Version string in the DLL and the ini header match the tag. Check: `acevo_perf.log` first line.

## Notes

- Binaries stay out of git, releases carry them.
