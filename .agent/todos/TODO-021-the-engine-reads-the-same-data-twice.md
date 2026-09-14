---
name: TODO-021-the-engine-reads-the-same-data-twice
kind: todo
description: the engine reads 3.5 GB of package data a second time in one session, 633 MB of it inside a single Nürburgring load, the same with the mod passive, and the owner counts that as wasted operation rather than load time
updated: 2026-09-14
links: [texture-streamer-flip-2026-09-13, TODO-013-faster-session-loads, directstorage-streaming, content-package, BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load]
status: open
by: owner
area: streaming
born: 2026-09-13
done:
---

## What

Owner wording, 2026-09-13, on the repeated reads being parked as load time: "Thats operation, not
load time. 1.08gb of operation is not ok. Not load time."

## Why

Boot 1 of 2026-09-13 (`logs/streamer-boot1-1124`, the `reread` rows of the streaming trace)
counted 11,143 file to memory reads, 3,554 MB, that repeat an earlier read of the same file,
offset and size exactly. The Nürburgring load repeated 1,598 MB, 633 MB of it a second read inside
that same load. The Red Bull Ring load repeated 592 MB, and each return to the menu 561 to 783 MB.
The largest are `sfx/free_roam.bank` 289 MB, the two track `.scene` files at about 100 MB each,
the Ferrari 296 GT3 meshes 96 and 81 MB, and `grass_3.scene` 91 MB. Round one of TODO-018 found the
same 1.08 GB inside a Nürburgring load with the mod passive, so it is the engine's. Disk reads, a
decoded copy in memory each time, and the CPU to decode and parse it. Full record in
[texture-streamer-flip-2026-09-13](../docs/research/texture-streamer-flip-2026-09-13.md).

The UI is one smaller instance. Its two stylesheets, 1.2 MB, are read and parsed again at every document
load, 33 and 34 times in a 20 minute race session, 38.2 MB of repeated reads, filed on its own as
[BUG-027](../bugs/BUG-027-ui-stylesheets-are-read-and-parsed-again-on-every-page-load.md) because Cohtml
has its own preload for it.

## Done when

It is known why the engine reads each of the big repeats again, whether a repeat costs a real
disk read (DirectStorage normally bypasses the Windows file cache, not yet checked on this path),
and the mod either stops the waste safely or the record says why it cannot.
