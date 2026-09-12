---
name: DEC-015-bundled-directstorage-core-loaded-first
kind: decision
description: the mod ships DirectStorage 1.3.0 as acevo_dstoragecore.dll and calls the core directly, because the game owns the name dstoragecore.dll and loads its own first
updated: 2026-09-12
links: [DEC-007-drag-and-drop-install-with-bundled-runtime, DEC-001-dstorage-proxy-as-loader, build-and-release]
date: 2026-09-12
area: streaming
status: standing
superseded-by:
---

## Decision

Ship Microsoft's DirectStorage 1.3.0 and use it instead of the 1.2.3 the game carries, as
`acevo_dstoragecore.dll` next to the exe, called directly by the proxy. `dstorage_orig.dll` stays
in the payload as the fallback. Nothing belonging to the game is replaced or renamed.

The mechanism matters because `dstorage.dll` is only a forwarder. The runtime lives in
`dstoragecore.dll`, which the forwarder finds by bare name in the game executable's folder, where
Assetto Corsa EVO keeps its 1.2.3 copy (byte identical to the 1.2.3 NuGet package). Windows treats
a module's base name as its identity, so the first `dstoragecore.dll` loaded owns the name and
every later load of any path gets that same module back. The exe references `dstoragecore.dll`
itself and loads it during start-up, well before it calls any of our exports, so there is no
ordering the proxy can win. Competing for the name is the wrong game.

So the proxy stops competing. Our copy ships under a name nothing else asks for and the proxy
calls its entry points itself, which is all the forwarder ever did: `DStorageGetFactory` resolves
`DStorageGetFactoryCore` and tail jumps to it with the same arguments, `SetConfiguration1` and
`CreateCompressionCodec` likewise, and plain `SetConfiguration` widens the older struct by one
field first. The core does not depend on the forwarder, and the game's runtime can sit loaded
beside ours without either noticing.

How much this buys, read off Microsoft's own changelog in the package rather than off a headline:
one fix between 1.2.3 and 1.3.0 lands on a path this game uses, `DSTORAGE_DESTINATION_TILES` for
resources whose width and height differ. That is the texture tile queue, and a short menu session
already put 2263 requests and 1.6 GB of tiles through it. Everything else misses: the CPU
decompression corruption fix and the 1.2.4 GPU decompression shader fix are compression paths, and
this game streams uncompressed on every queue (`compressed=0 gdeflate=0`), while `IDStorageQueue3`
and the new destination type are functions for a title to call and this one is built against 1.2.
The file IO race and the decompression threadpool deadlock people associate with this era are
1.2.3 fixes, which the game already has. So this is being on the current runtime with one relevant
fix, not a measured speed up, and it should not be sold as one.

## Alternatives

- Ship only the newer forwarder and leave the core alone: measured, and it is the trap. A 1.3.0
  forwarder on a 1.2.3 core returns `S_OK`, creates queues, reports nothing wrong and quietly runs
  the old code. There is no version check between the two. The upgrade would have looked done and
  changed nothing, which is why the mod reads the loaded core's `DStorageSDKVersion` back and
  writes it to the log instead of trusting what was shipped.
- Ship the core as `dstoragecore.dll` in a subfolder and load it by full path before the forwarder
  looks: this was built, shipped to the game folder and tried, and the log caught it running 1.2.3
  anyway. It works in a harness and loses in the game, because the game loads its own copy during
  start-up and the base name is already taken by the time any of our code runs. Worth recording
  because it looks correct and the only thing that exposed it was reading the version back.
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

- The payload gains one file and about 1.4 MB. Install is still a copy, uninstall is still a
  delete, and a game update cannot undo the upgrade because the game's own file was never the one
  being used.
- Two DirectStorage runtimes are resident, the game's and ours. Only ours is ever initialised, a
  loaded module does nothing until an entry point is called, so the cost is the mapping.
- Every path is a working path. `bundled_runtime=0`, a missing `acevo_dstoragecore.dll`, or a core
  without the expected entry points all fall back to `dstorage_orig.dll` and the game's runtime,
  and say so in the log with the version read back from the module that really loaded.
- The proxy now depends on three exports Microsoft does not document,
  `DStorageGetFactoryCore`, `DStorageSetConfigurationCore` and `DStorageCreateCompressionCodecCore`.
  Their signatures were taken from the forwarder's own disassembly, where each is a tail jump with
  the arguments untouched. If a future runtime drops or changes them the resolve fails, the log
  says so and the fallback carries the game.
- The proxy now declines `IDStorageQueue3`. It does not implement it, and handing the real queue
  over would put the game on the runtime directly, past the overlay's file redirection and past
  the statistics. The game is built against 1.2 and never asks. If a later build does, the log
  says so and the answer is to implement `EnqueueRequests` in the proxy (TODO-014).
- The repo now carries two Microsoft binaries instead of one, both on the package's
  `distributable_files.txt`, with the licence files already beside them.
