---
name: dstorage-dll-is-only-a-forwarder
kind: memory
description: dstorage.dll only forwards to dstoragecore.dll, which holds the runtime and which the game claims by name at start-up, and nothing checks that the two versions match
updated: 2026-10-08
links: [DEC-015-bundled-directstorage-core-loaded-first, DEC-007-drag-and-drop-install-with-bundled-runtime, build-and-release, directstorage-1-3-2026-09-12]
type: project
---

`dstorage.dll` holds no runtime. Each of its exports forwards to a `...Core` entry point in
`dstoragecore.dll`, which the game loads from its own folder during start-up, before it calls
anything of ours, and nothing checks that the forwarder and the core are the same version.

It matters because replacing `dstorage.dll`, which is all the proxy is, never changes the runtime,
and a mismatched pair returns `S_OK` and runs the old code with no warning. Loading our own core as
`dstoragecore.dll` ahead of the forwarder cannot work either, since the game already owns that name.
That attempt was built and shipped, and only the version read back off the loaded module showed it
had failed.

Apply it by keeping the mod's core under its own name, `acevo_dstoragecore.dll`, called directly
(DEC-015), and by trusting only the `[runtime] DirectStorage 1.x.y in use` line in `acevo_perf.log`
after any change to the runtime files. How the forwarder finds its core and how the version number
reads are in [directstorage-1-3-2026-09-12](../docs/research/directstorage-1-3-2026-09-12.md), the
install side in [build-and-release](../docs/ops/build-and-release.md).
