---
name: directstorage-streaming
kind: doc
description: how the game streams through DirectStorage, how its texture streamer decides and how VRAM pools are sized
updated: 2026-09-13
links: [proxy-architecture, DEC-003-staging-buffer-128mb, DEC-009-pool-and-staging-sizes-by-card, DEC-017-streamer-reload-fix-refuses-the-drop, texture-streamer-flip-2026-09-13, texture-streamer-overload-2026-09-13, game-requests-1gb-staging-buffer, content-package]
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

## The texture streamer

About once a second the streamer runs a kick. It takes one frame of demand, gives every (texture,
level) a priority from material distance tables or GPU feedback, admits in priority order until the
pool budget runs out, loads what was admitted if the load gate has space, and drops the rest at
once. The tile queue carries whole standard mips, the file to memory queue the packed tail. A drop
frees its tiles to a first in first out pool two frames later with no unmap. The pool runs full in
normal play, 14,800 to 15,300 of 16,384 tiles used at 1024 MB on a 6 GB card, with about 120 loads
turned away for space on every parked kick, and at 15,360 through a race with AI.

Three things go wrong in it, each fixed from `src/engine/streamer.cpp`, which also writes the
streaming trace and the census, all three on by default in `[engine]`.

- The feedback is measured against the mip that is loaded and read as if against the full texture,
  so textures on the budget edge drop and reload the same mip every two kicks.
  `streamer_reload_fix` refuses that drop
  ([DEC-017](../../decisions/DEC-017-streamer-reload-fix-refuses-the-drop.md)), with the evidence in
  [texture-streamer-flip-2026-09-13](../research/texture-streamer-flip-2026-09-13.md).
- A texture drawn by several objects keeps the lowest of their priorities, so the player's car and
  shared track textures rank as their least important use. `streamer_rank_fix` keeps the highest.
- A load starts only when every missing level fits the free space at once, so big textures wait at
  their coarsest level while small ones take the space. `streamer_partial_loads` loads the levels
  that fit. The last two are in
  [texture-streamer-overload-2026-09-13](../research/texture-streamer-overload-2026-09-13.md),
  with the priority tables and who holds the pool in a race.

## How files are requested

Probe of 2026-09-05 (menu launch, every request logged): the package's table of contents is not
read through DirectStorage, no request touches the last 64 MB of the file, so the engine reads it
with ordinary file I/O at startup. Every `FileToMemory Queue` request is one whole package entry,
offset equal to the entry offset and size equal to the entry size (11,214 requests, all resolved
to entries, none partial). UI images (`uiresources\images\...texturemips`) arrive as 16 requests
per file on the same queue. The largest single requests are audio banks (84 MB) and car interior
meshes (34 MB).

## VRAM pool sizing

The game never calls `DStorageSetConfiguration` and calls `SetStagingBufferSize(1024 MB)`. The
runtime keeps two staging buffers of that size in local video memory. After its own allocations
the engine (`DeviceAllocator.cpp`) computes `[Tile Pool] remainder N MB -> texture pool N/2.5`
and the same for the mesh streamer. On a 6 GB card:

| staging buffer | "other objects" VRAM | texture pool | mesh pool |
|---|---|---|---|
| 1024 MB (game) | 2343 MB | 400 MB | 400 MB |
| 256 MB | 807 MB | 1015 MB | 1015 MB |
| 128 MB | 551 MB | 1115 MB | 1115 MB |
| `force_canonical_pool_sizes` alone | | 1433 MB | 1433 MB |
| 128 MB plus `force_canonical_pool_sizes` and `tile_pool_mb=1024` (mod default) | 500 MB | 1024 MB fixed | 1433 MB cap |

The dynamic formula has a second problem beyond the staging buffers: it runs during the scene
transition while the outgoing scene is still resident, so those menu numbers do not survive a
race. Measured on the same card, dynamic path: menu 1117 MB, race load 633 MB, session restart
526 MB, with a gigabyte of VRAM unused in the race (BUG-010). With the two flags the tile pool is
created once at `tile_pool_mb` and never resized, and the mesh budget is a 1433 MB cap that fills
on demand. Lap four of 2026-09-05 with that default and texture quality Ultra: 4556 to 4614 MB
in use while driving, one second at 5222 MB during the race load, budget 5226 MB.

Since 2026-09-06 both sizes default to `auto` (DEC-009): the proxy reads the render adapter's
dedicated memory off the first DXGI factory the game creates and picks 1024, 1536, 2048 or
3072 MB of tiles and 128, 192 or 256 MB of staging for cards under 7, 11 and 15 GB and above,
so the 6 GB numbers above are what a 6 GB card still gets.

## Lap behaviour

On the Nordschleife lap of 2026-09-05 (14 minutes driving) the tile queue moved 13,314 requests
and 8.3 GB, median batch 5 per submit, peak 761 MB in one second, while VRAM peaked at 4592 of
5226 MB. The I/O path is never the limit, see the research doc `lap-2026-09-05-nordschleife`.
