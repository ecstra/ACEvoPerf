---
name: TODO-004-release-packaging
kind: todo
description: one zip and one script so anyone can install the mod without building
updated: 2026-09-05
links: [build-and-release]
status: done
by: owner
area: release
born: 2026-09-05
done: 2026-09-05
---

## What

"I hope you are making sure that you are following the best practices (so that anyone can
install this as a mod later)."

## Done when

A `release.ps1` at the root runs `build.ps1` and zips the payload with a version stamp, and a
fresh machine installs by copying the files into the game folder. Done 2026-09-05: the zip holds
`dstorage.dll`, `dstorage_orig.dll`, `acevo_perf.ini` and `README.txt`, install is drag and drop
(DEC-007, the owner's requirement replaced the installer script), the DLL carries a version
resource that names the zip, verified by extracting the zip over the game folder and launching.
