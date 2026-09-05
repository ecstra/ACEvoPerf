---
name: directstorage-streaming
kind: doc
description: how the game streams through DirectStorage and how VRAM pools are sized
updated: 2026-09-05
links: [proxy-architecture, DEC-003-staging-buffer-128mb, game-requests-1gb-staging-buffer, content-package]
---

# DirectStorage streaming

Observed through the proxy on 0.9.0 (the `CreateQueue`, `OpenFile` and `[stats]` lines of
`acevo_perf.log`).

## Queues

Three queues, all at capacity 8192 (the maximum), priority normal, no compression on any request.

| queue | source and destination | 75 second menu session |
|---|---|---|
| `FileToMemory Queue` | package to CPU memory, then XOR decoded on the CPU | 11,448 requests, 1.23 GB, largest 82 MB |
| `GpuUpload Memory Queue` | CPU memory to buffers and texture regions | 11,041 requests, 854 MB, largest 32 MB |
| `GpuUpload File Queue` | package straight into reserved resource tiles | 1,373 requests, 798 MB, largest 16 MB |

Textures are 64 KB tiled resources (`TilingInfo` in `TextureMetadata`, see content-package).
The engine decides which tiles to request from a GPU texture feedback pass (render target
`main_streamer_feedback_depth`, staging buffers `feedbackStagingBuffer0` and `1` in the exe), so
a request follows the first frame that needs the mip by at least two frames.

## VRAM pool sizing

The game never calls `DStorageSetConfiguration` and calls `SetStagingBufferSize(1024 MB)`. The
runtime keeps two staging buffers of that size in local video memory. After its own allocations
the engine (`DeviceAllocator.cpp`) computes `[Tile Pool] remainder N MB -> texture pool N/2.5`
and the same for the mesh streamer. On a 6 GB card:

| staging buffer | "other objects" VRAM | texture pool | mesh pool |
|---|---|---|---|
| 1024 MB (game) | 2343 MB | 400 MB | 400 MB |
| 256 MB | 807 MB | 1015 MB | 1015 MB |
| 128 MB (mod default) | 551 MB | 1115 MB | 1115 MB |
| `force_canonical_pool_sizes` | | 1433 MB | 1433 MB |

## Lap behaviour

On the Nordschleife lap of 2026-09-05 (14 minutes driving) the tile queue moved 13,314 requests
and 8.3 GB, median batch 5 per submit, peak 761 MB in one second, while VRAM peaked at 4592 of
5226 MB. The I/O path is never the limit, see the research doc `lap-2026-09-05-nordschleife`.
