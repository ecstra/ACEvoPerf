---
name: game-requests-1gb-staging-buffer
kind: memory
description: the game asks DirectStorage for a 1024 MB staging buffer and the runtime keeps two in VRAM
updated: 2026-09-05
links: [DEC-003-staging-buffer-128mb, directstorage-streaming]
type: project
---

The game calls `IDStorageFactory::SetStagingBufferSize(1024 MB)` once, right after creating the
factory, and never calls `DStorageSetConfiguration`. The runtime allocates two staging buffers of
that size in local video memory. Read from the proxy log on 0.9.0, confirmed by the engine's own
`QueryVideoMemoryInfo` line moving by exactly two times the size change.

It matters because the engine sizes its texture and mesh streaming pools from the VRAM left
after everything else, so this one call decides texture quality on small cards.

Apply it by keeping the cap in the proxy and by re checking the `game called
SetStagingBufferSize` line in `acevo_perf.log` after every game update.
