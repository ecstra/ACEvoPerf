---
name: DEC-015-bundled-directstorage-core-loaded-first
kind: decision
description: the mod ships DirectStorage 1.3.0 and loads its core before the forwarder can find the game's 1.2.3 one, no game file is replaced
updated: 2026-09-12
links: [DEC-007-drag-and-drop-install-with-bundled-runtime, DEC-001-dstorage-proxy-as-loader, build-and-release]
date: 2026-09-12
area: streaming
status: standing
superseded-by:
---

## Decision

Ship Microsoft's DirectStorage 1.3.0 in the zip as two files, the forwarder as `dstorage_orig.dll`
next to the exe and the runtime itself as `acevo_perf\dstoragecore.dll`, and have the proxy load
that core by full path before it loads the forwarder. Nothing belonging to the game is replaced.

The split matters because `dstorage.dll` is only a forwarder. The runtime lives in
`dstoragecore.dll`, which the forwarder loads by bare name, from the game executable's own folder,
where Assetto Corsa EVO keeps its 1.2.3 copy (byte identical to the 1.2.3 NuGet package). Windows
hands a bare name `LoadLibrary` whatever module is already loaded under that name, so loading ours
first is enough to win. That was measured rather than assumed, both against a copy sitting next to
the exe and against `LOAD_LIBRARY_SEARCH_SYSTEM32`, which is the search the 1.3.0 forwarder tries
first and which a future Windows with an inbox runtime would answer.

The reason to move at all is 1.2.4, which fixed a race that could stop DirectStorage processing
requests while the CPU is under heavy load. A session load in this game is exactly that: a quarter
of a busy resource worker's time goes into the engine's job queue spin loop (TODO-013). 1.3.0
carries that fix plus a year of runtime work. Its own new API surface is for the title to call, so
none of it reaches this game.

## Alternatives

- Ship only the newer forwarder and leave the core alone: measured, and it is the trap. A 1.3.0
  forwarder on a 1.2.3 core returns `S_OK`, creates queues, reports nothing wrong and quietly runs
  the old code. There is no version check between the two. The upgrade would have looked done and
  changed nothing, which is why the mod now reads the loaded core's `DStorageSDKVersion` back and
  writes it to the log instead of trusting what was shipped.
- Replace the game's `dstoragecore.dll` and keep a `dstoragecore_orig.dll` next to it: the owner
  was happy with this. It loses anyway. Nobody can ship the backup copy, so the user has to rename
  a file before copying, which is the install script DEC-007 removed. A game update restores 1.2.3
  and silently undoes the upgrade. Steam's verify files fights it every time.
- 1.4.0: preview only, and it holds nothing for this game. Zstd is a format the game does not use,
  and `DSTORAGE_CONFIGURATION2` adds one field, a `CreatorID` GUID that labels D3D12 queues for
  profiling tools.
- Use `IDStorageQueue3::EnqueueRequests` from the proxy to batch what the game enqueues one at a
  time: real, and not worth it. The drive was about a second of a thirteen second load, so per
  request runtime overhead is not what is costing the time, and replaying a batch means owning the
  ordering between requests and the fence signals the game interleaves with them.

## Consequences

- The payload gains a folder, `acevo_perf\`, and about 1.4 MB. Install is still a copy, uninstall
  is still a delete, and a game update cannot undo the upgrade because the game's own file was
  never the one being used.
- Every path is a working path. `bundled_runtime=0`, a missing folder, or a user who copies only
  the DLLs all fall back to the game's runtime and say so in the log with the version read back
  from the module that really loaded.
- The proxy now declines `IDStorageQueue3`. It does not implement it, and handing the real queue
  over would put the game on the runtime directly, past the overlay's file redirection and past
  the statistics. The game is built against 1.2 and never asks. If a later build does, the log
  says so and the answer is to implement `EnqueueRequests` in the proxy (TODO-014).
- The repo now carries two Microsoft binaries instead of one, both on the package's
  `distributable_files.txt`, with the licence files already beside them.
