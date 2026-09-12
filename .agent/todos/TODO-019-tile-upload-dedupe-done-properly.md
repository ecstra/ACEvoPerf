---
name: TODO-019-tile-upload-dedupe-done-properly
kind: todo
description: the tile upload dedupe works and removes 1.4 GB of disk reads and GPU uploads per lap, and was dropped only because its free list runs dry and the fallback can collide, so it comes back as its own round with the eviction signal solved first
updated: 2026-09-12
links: [tile-pool-reshuffle-2026-09-12, TODO-018-look-properly-at-the-streaming-layer, directstorage-streaming, BUG-016-vram-overhead-grows-across-scene-loads]
status: open
by: owner
area: streaming
born: 2026-09-12
done:
---

## What

Owner wording, 2026-09-12: "we do have to fix dedupe properly since it saves a significant chunk
on disk and ram and GPU. so thats later, just not now (it gets its own proper full round)."

## Why it is worth a round

It was built on 2026-09-12, it worked, and it was dropped for one specific reason that is
solvable. Over a single Nürburgring lap:

| | |
|---|---|
| uploads dropped | 1,542 of 6,650 |
| bytes dropped | **1,435 MB of 5,274, 27 percent** |
| visible corruption | none |

That is disk reads, staging buffer traffic and GPU upload bandwidth, all removed, permanently,
on every lap. It did not show up as frame rate on this thermally pinned card, which is why it was
not shipped in a hurry, but it is real work that does not need doing.

## Why it was dropped, which is the thing to fix first

With the dedupe live the mod owns tile placement in the pool. Its free list only refills when the
engine signals an eviction, and the engine signals fewer evictions than it consumes slots. The
session ended at **16,096 tiles held and 286 free of 16,384, 98 percent consumed.** On exhaustion
the code falls back to the engine's chosen slot, which may already hold one of ours, and a
collision puts the wrong texture on screen.

One lap did not reach it. A longer session or a track change very likely would.

**So the round starts with the eviction signal, not the dedupe.** The dedupe is already written
and proven. The open question is how the engine expresses "this tile is no longer wanted", given
it maps 68,051 tiles and unmaps 21 in a whole session. Until that is answered the free list
cannot be kept honest, and everything else is premature.

Minimum acceptable behaviour whatever the answer: when the free list runs dry, the dedupe turns
itself off for the rest of the session and says so in the log. A graceful stop instead of a cliff.

## What was built, so none of it is re-derived

- **The hook.** `ID3D12CommandQueue::UpdateTileMappings` is vtable slot 8 and `CopyTileMappings`
  slot 9, counted the same way the shading rate slots were and confirmed by the disassembly at
  `0x268` and `0x270`. The queue comes from the DXGI swap chain hook the mod already has.
- **The architecture that works.** The mod must own allocation outright. Rewrite the heap offsets
  in the mapping call so a tile stays where its data already is, then drop the matching upload in
  `EnqueueRequest`. Follow the engine's own slot choices only as the eviction signal.
- **Calls it may rewrite.** One linear region, one range per tile, a real heap, no NULL, SKIP or
  REUSE_SINGLE_TILE flags. Anything else passes through untouched and the maps follow the engine
  exactly, so the two views cannot drift.
- **Dropping the request is safe for synchronisation.** Status arrays and fences are enqueued
  separately, so a fence still signals after the remaining requests.

## Two bugs found by hand before it ever ran

Both would have shown as wrong textures rather than a crash. Anything rebuilt here inherits them.

1. **Redirecting a tile to its old slot is certain to corrupt.** The engine then believes that
   slot is free and hands it to another tile, two tiles point at one slot, and the second upload
   overwrites the first. This is why owning allocation is not optional.
2. **Stale slot ownership.** When a tile moves from engine slot 100 to 200, slot 100 keeps
   pointing at it unless cleared, so the next tile to take slot 100 looks like an eviction of a
   tile that is still alive and frees its data from under it. Track the engine slot per tile and
   clear it on the move.

## Done when

The free list stays honest across a long session and a track change, the dedupe stops itself
gracefully rather than colliding if it ever cannot, and a multi lap session with it on shows no
visual defect. Then it ships behind an ini key, off by default until it has run clean for a while.
