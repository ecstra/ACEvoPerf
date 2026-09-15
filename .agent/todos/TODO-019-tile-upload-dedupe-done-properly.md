---
name: TODO-019-tile-upload-dedupe-done-properly
kind: todo
description: the round for the redundant tile traffic that is left, since the parked churn is now fixed at its source, which is driving, 97 percent of 20 GB in ten minutes at the Red Bull Ring asking again for mips already requested, with the 2026-09-12 dedupe's cliff shown impossible and its real wrong texture defects named
updated: 2026-09-14
links: [texture-streamer-flip-2026-09-13, DEC-017-streamer-reload-fix-refuses-the-drop, tile-pool-reshuffle-2026-09-12, TODO-018-look-properly-at-the-streaming-layer, directstorage-streaming, BUG-016-vram-overhead-grows-across-scene-loads, texture-streamer-camera-cuts-2026-09-14, TODO-024-a-follow-up-streamer-pass-after-a-camera-cut]
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

## What 2026-09-13 changed

TODO-018's round, [texture-streamer-flip-2026-09-13](../docs/research/texture-streamer-flip-2026-09-13.md),
rewrote most of what this round was going to start from.

- **The cliff it was dropped for cannot happen.** In that code held plus free always equalled the
  high water mark, and a replay of the dedupe over a recorded session reached the fallback zero
  times. "16,096 held, 98 percent consumed" counted slots of textures that were already dead.
- **It had two real wrong texture defects instead.** It keyed tiles by resource pointer, and the
  engine reuses pointers for different textures, 114 uploads it would have dropped carried
  different bytes. And it shared one slot map between the texture pool and the tiled instances
  heap, which a replay showed colliding. "No visible corruption" over one lap was luck.
- **The eviction signal is answered.** The engine never unmaps a texture tile. A drop puts the
  indices on the pool's first in first out free queue two frames later, and the pool's used and
  pending counters can be read live, `src/engine/streamer.cpp` already does.
- **The parked churn is fixed at its source.** `streamer_reload_fix` refuses the drop behind it,
  measured at zero parked tile requests in boot 2 ([DEC-017](../decisions/DEC-017-streamer-reload-fix-refuses-the-drop.md)).
  A dedupe would only have hidden those uploads while the streamer kept dropping and remapping.

What is left is driving. Ten minutes at the Red Bull Ring put 20,061 MB through the tile queue,
33.6 MB/s, and 19,447 MB of it asked again for a resource and mip already requested in the stint.
The reload fix reaches about a fifth of that by replay, because a big ground texture's screen
coverage changes every kick while moving and its pin lets go.

**So the round starts from boot 1's trace, `logs/streamer-boot1-1124`, not from the dedupe.** Split
those 19.4 GB into feedback flips the fix released, flips with no fresh feedback, and genuine
changes of view, then decide between widening the fix and a dedupe rebuilt on source identity.
Either way, replay it through that trace before it costs the owner a session.

## What the camera cut dive adds, 2026-09-14

From [texture-streamer-camera-cuts-2026-09-14](../docs/research/texture-streamer-camera-cuts-2026-09-14.md).

- **Camera cuts are a traffic source of their own.** In the Red Bull Ring pit menu showcase the reloads
  after cuts run 9 to 13 MB/s at either pool size, a whole car of about 6,750 tiles dropped and loaded
  again every cycle. Keeping textures across cuts would save 72 percent of them at 1536 MB and 7 percent
  at 1024 MB. That work sits in TODO-024 and BUG-021.
- **No duplicates in the pit menu.** 5 repeated requests of a held level in 330 s, and a pool rebuilt
  from the trace stays within 8 tiles of the engine's counter.
- **Cross run traffic numbers are unreliable.** Two identical 29 AI races differ by 28 percent in tile
  traffic while their refusals match, so this round's measure has to be inside one run or a replay.

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

The driving re-request traffic is split by cause from a recorded trace, and whichever fix is
chosen, a widened reload fix or a dedupe keyed on source bytes and per heap, removes a measured
share of it while a multi lap session shows no visual defect. Then it ships behind an ini key, off
by default until it has run clean for a while. Or the record says why the rest cannot be removed
safely.
