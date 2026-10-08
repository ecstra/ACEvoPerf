---
name: TODO-029-stop-shipping-dstorage-orig-and-fall-back-to-the-games-own-core
kind: todo
description: drop dstorage_orig.dll from the zip and point the fallback at the game's own dstoragecore.dll directly, which the proxy already knows how to call, so the payload loses a file that confuses uninstall and gives up nothing the fallback still provided
updated: 2026-10-08
links: [DEC-015-bundled-directstorage-core-loaded-first, build-and-release, DEC-007-drag-and-drop-install-with-bundled-runtime, public-docs]
status: done
by: owner
area: release
born: 2026-09-20
done: 2026-10-08
---

## What

In the owner's words, after the uninstall wording came up on 2026-09-20: "If we are doing that anyways,
why dont we stop shipping the _orig.dll from 0.4?"

Not just stop shipping it. Replace what it does. `EnsureReal` in `src/dstorage/proxy.cpp:90` falls back to
loading `dstorage_orig.dll`, which is Microsoft's forwarder, which resolves `dstoragecore.dll` by bare
name and gets whichever module owns that name, which is always the game's own. Point the fallback at
`dstoragecore.dll` directly and call its core entry points, exactly as `LoadBundledCore` already does for
`acevo_dstoragecore.dll`. The forwarder was only ever doing the resolve and jump the proxy now does
itself, which is the whole of DEC-015.

After that, `release.ps1` drops the file from the payload, `build.ps1` stops copying it into `dist/`, and
the readme, the zip readme and the ops docs lose it from the file lists and the uninstall steps.

Deferred by the owner until the full review of main is finished, on the day it was raised.

## Why

The measurement that settles the cost. The game's own core, version 1.2.2407.1501, exports exactly four
symbols: `DStorageGetFactoryCore`, `DStorageSetConfigurationCore`, `DStorageCreateCompressionCodecCore`
and `DStorageSDKVersion`. There is no `DStorageSetConfiguration1Core`. So the shipped forwarder cannot
resolve `DStorageSetConfiguration1` against it either, and the fallback path has been missing that call
all along, whether it is reached through `bundled_runtime=0` or through the forwarder. Going direct gives
up nothing that was still there.

What it buys:

- The zip drops to four files and about 200 KB
- Uninstall becomes delete `dstorage.dll` and everything starting with `acevo_`, then verify in Steam,
  with no file left that looks like it wants renaming back. That confusion is live: the Overtake listing
  carried the 0.3.1 era rename instruction until the owner corrected it on 2026-09-20
- `bundled_runtime=0` starts doing what its log line already claims, using the game's own runtime,
  instead of routing through a Microsoft forwarder to reach it
- One less Microsoft binary redistributed. `acevo_dstoragecore.dll` keeps the licence and notices
  obligation that `sweep/review-public-docs` F-14 raises, so this halves that surface rather than
  closing it

What it gives up is the case where `acevo_dstoragecore.dll` is missing or blocked and the game's core is
somehow not loaded. The game loads its own core during start-up before it ever calls one of our exports,
which is the fact DEC-015 rests on, so that window does not really exist. Worth stating in the branch
rather than assuming.

## Done when

`dstorage_orig.dll` is gone from `build.ps1`, `release.ps1`, `dist/`, the zip, `README.md`,
`dist/README.txt` and `.agent/docs/ops/build-and-release.md`, and a launch shows the mod working on both
settings: `bundled_runtime=1` still logging the 1.3 runtime in use, and `bundled_runtime=0` logging the
game's own and reaching a playable session. Its own branch off the open version branch, because it
changes the load path rather than the paper around it.

Done on 2026-10-08 on `sweep/drop-dstorage-orig`. `64704e1` points the fallback at the game's own
`dstoragecore.dll` through the same three core entry points the bundled core uses, and drops the
forwarder's runtime report and the plain configuration path only it ever set. `ac11680` drops the file
from the build and the zip, `f064300` from the readmes and the changelog, which keep it in the uninstall
step only for a leftover, and `85cd5dc` from the docs and DEC-015.

Verified with the forwarder moved out of the game folder, `logs/drop-orig-20261008`. With
`bundled_runtime=0` the log read `DirectStorage 1.2.3 in use, from dstoragecore.dll`, the configuration
call returned 0, which it never did through the forwarder, the Nurburgring loaded and played, and the quit
was clean. With `bundled_runtime=1` it read `DirectStorage 1.3.0 in use, from acevo_dstoragecore.dll`.
No exception in either game log.
