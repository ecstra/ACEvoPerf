---
name: texture-streamer-flip-2026-09-13
kind: doc
description: the tile churn is the texture streamer measuring detail against the mip it has loaded and reading the answer as if against the full texture, confirmed live, fixed from the mod behind an off by default switch, with the pool found full in normal play, 33.6 MB/s of repeat traffic while driving at the Red Bull Ring and 3.5 GB of repeated file reads a session
updated: 2026-09-13
links: [TODO-018-look-properly-at-the-streaming-layer, TODO-019-tile-upload-dedupe-done-properly, TODO-021-the-engine-reads-the-same-data-twice, DEC-017-streamer-reload-fix-refuses-the-drop, tile-pool-reshuffle-2026-09-12, directstorage-streaming, telemetry, BUG-016-vram-overhead-grows-across-scene-loads, TODO-017-tier-2-variable-rate-shading]
---

# The texture streamer measures detail against the mip it has loaded

TODO-018's round. Two offline rounds read the exe, its shaders and every log of 2026-09-12, with
each claim re-derived by a separate verifier, then two boots of one build on 2026-09-13 put the
model in front of the game. Sessions `logs/streamer-boot1-1124` and `logs/streamer-boot2-fix-1141`.

## How the streamer decides

The texture streamer sits at `Renderer+0x978`. Every RVA below is for the 0.9.1 exe and the ones
the mod depends on are pinned by hash in `src/engine/streamer.cpp`.

- **The kick.** A job at `0x1f2d110` runs when a 1000 ms deadline expires, re-armed once the
  previous kick has finished, so kicks land every 1.017 s. A kick is also forced when used plus
  pending tiles exceed the pool.
- **Demand is one frame.** Requests are collected for exactly one frame per kick and nothing
  carries over.
- **Admission.** Every (texture, level) gets a 16 bit priority, from material distance tables or
  from GPU feedback. Records are admitted in priority order against the budget and the walk stops
  at the first that does not fit. There is no hysteresis.
- **Loads and drops.** An admitted texture below its level loads if the load gate has space for
  it, which is the pool's free tiles minus pending minus a 1024 tile margin. A texture above its
  admitted level, or not admitted at all, is dropped at once. Drops are never gated.
- **Release.** A drop pushes the tile indices into a per frame bucket that reaches the pool's free
  queue two frames later. The queue is first in first out, so a reloaded mip always lands on
  different slots. No `UpdateTileMappings` unmap is ever issued for a texture tile.

## The defect

The 17 material shaders that write feedback compute the level of detail from `GetDimensions(0)`
of the view bound in the texture's bindless slot. The engine creates one view per streaming level
and copies the current one into that slot on every load and drop. So the mip they write is
measured against the mip that is loaded. The kick converts it with
`fbLevel = clamp(levels - 1 - mip)` and no reference to the loaded level (`0x1f2dc2a..0x1f2dc4b`),
as if it had been measured against the full texture.

A texture whose finer level fits the budget therefore cannot settle. With the coarse level loaded
the reading is small, often the clamped zero, and asks for the finer level. With the finer level
loaded the same view reads larger and asks for the coarse one. Load, drop, every two kicks.

The shader cannot say "finer than this view", its reading stops at zero, so correcting the
conversion would stop textures sharpening. That is why the fix refuses the drop instead,
[DEC-017](../../decisions/DEC-017-streamer-reload-fix-refuses-the-drop.md).

**Confirmed live in boot 1.** Parked at the Nürburgring pit exit every churning texture's fresh
reading moves by exactly the levels it gained or lost.

| texture | levels | finer state reads | coarser state reads |
|---|---|---|---|
| `terrain_new_far` | level 4 and 2 of 5 | mip 2 | mip 0 |
| `grail_tess_nm` | level 3 and 1 of 4 | mip 2 | mip 0 |
| `details_alpha` | level 4 and 3 of 5 | mip 1 | mip 0 |
| `marshal_boxes_dif` | level 1 and 0 of 4 | mip 3 | mip 2 |

Over the whole Nürburgring stint there were 1,113 load then drop pairs. The finer state read 1
higher in 489 of them, 2 higher in 163, 3 higher in 64, the same in 71, and 263 had a stale reading.

## The pool is full in normal play

The parked kicks of boot 1 repeat to the tile. Capacity 16,384, budget 13,599, 2,780 records of
which 1,406 or 1,419 are admitted for 13,593 or 13,564 tiles, and 157 to 322 tiles of load gate
space.
About 174 textures want a finer level every kick and about 120 of those loads are turned away for
space, every kick. The pool ran at 14,800 to 15,300 of 16,384 tiles used through both tracks, and
39,592 loads were turned away for space in a 17 minute session.

That decided the fix's shape. A first rule stood down whenever gate space was short or any load
had been turned away, and replayed through boot 1 it would have acted 28 times.

## The fix

`[engine] streamer_reload_fix`, off by default. The mod recognises the flip from the readings, a
drop on a fresh reading followed within six kicks by a reload on a fresh reading no higher than
that one shifted down by the levels lost. That texture's next drop back is refused. It lets go when a drop
leaves the flip, the reading goes stale, the reading says the view moved away, the texture's screen
coverage in feedback samples moves by more than a quarter, or the texture is not admitted at all.
A drop is also let through, without forgetting the flip, whenever used plus pending tiles no longer
leave the engine's 1024 tile margin free, since that margin is what loads outside the streamer's
gate live on. How it hooks the exe is in the source comment of `src/engine/streamer.cpp`.

**Replayed through boot 1** before it ran.

| stretch | reload traffic | removed |
|---|---|---|
| Nürburgring parked minute | 883 MB requested | all of it |
| Nürburgring lap | 1,703 MB | 465 MB, 27 percent |
| Red Bull Ring, 10 minutes driving | 16,936 MB | 3,605 MB, 21 percent |

**Boot 2, fix on.** Parked at the same pit exit, tile requests were zero for the whole parked
stretch, against 12 to 15 MB/s in boot 1's parked minute, while the fix refused the same 19 drops
on every kick. Over the session it refused 1,401 drops and let go 690 times (coverage 250, left the
flip 172, not admitted 124, moved away 99, stale 45), and the margin let 17 through. No hook
faulted. The owner watched the picture over a lap and reported "nothing looks wrong per say".

Frame rate is not claimed. Boot 2 parked at about 90 fps on a fresh launch and boot 1 at about 82
straight after ten minutes of hotlapping, with no GPU sampler running in either, and the process
used the same 25 percent CPU both times. The churn was measured at about 4 percent of frame time on
2026-09-12, so a gain of that size would be consistent, but these two boots cannot show it.

## Driving at the Red Bull Ring

Ten minutes of hotlaps put 14,007 tile requests and 20,061 MB through the tile queue, 33.6 MB/s,
and 19,447 MB of that asked again for a resource and mip it had already requested in the stint. The
largest are `map2` 1,725 MB, `grass_top_normal_b` 961 MB, `top1` 908 MB, `top` 772 MB and the two
`grass_2` maps at 740 MB each. `map2` is a feedback flip like the parked ones. The fix reaches a
fifth of this, because while driving a big ground texture's coverage changes every kick and the pin
lets go, and because much of the rest flips with no fresh feedback at all. What is left belongs to
[TODO-019](../../todos/TODO-019-tile-upload-dedupe-done-properly.md).

The Nürburgring stint for comparison was 2,597 MB at 14.6 MB/s, led by `terrain_new_far` 410 MB
and `details_alpha` 291 MB. The first menu minute reloaded 6.6 MB, so there is no loop worth the
name in the menu.

## Repeated file reads

Boot 1 read 3,554 MB from the file to memory queue that repeated an earlier read of the same file,
offset and size exactly, 11,143 reads. The Nürburgring load alone repeated 1,598 MB, 633 MB of it a
second read inside that same load. The largest are `sfx/free_roam.bank` 289 MB, the two track
`.scene` files at about 100 MB each, the Ferrari 296 GT3 interior and exterior meshes 96 and 81 MB,
and the Nürburgring `grass_3.scene` container 91 MB. Filed as
[TODO-021](../../todos/TODO-021-the-engine-reads-the-same-data-twice.md). A load issues no repeated
tile requests, 623 at the Nürburgring with none identical.

## What 2026-09-12 got wrong

Each was reached from real data. The corrected record is
[tile-pool-reshuffle-2026-09-12](tile-pool-reshuffle-2026-09-12.md).

- **"The re-upload is genuinely required."** The drop behind it is the defect above. Refusing the
  drop removes the relocation and the upload together.
- **"Every 2.2 seconds."** Two kicks, 2.034 to 2.038 s. The 2.2 s average counted skipped cycles.
- **"68,051 mapped, 21 unmapped, the engine never releases a tile."** Counted right and misread.
  The 21 are one call on the tiled instances buffer, a separate 4096 tile pool, releasing the
  dealership scene's instance tiles at the Nürburgring load. Texture tiles are released on every
  drop, silently.
- **"The pool runs 92 to 98 percent full" and "16,096 held, 286 free".** A count of slots whose
  last texture had not moved, dead ones included. The engine's own counter, read live on
  2026-09-13, is 14,800 to 15,300 used.
- **The dedupe's free list cliff.** Impossible in that code, held plus free always equalled the
  high water mark and a replay reached the fallback zero times. Its real defects were keying tiles
  by resource pointer, which the engine reuses for different textures (114 such uploads with
  different bytes), and sharing one slot map between the texture pool and the instances heap.
- **"No churn at a 405 MB pool."** That control also cut the mesh budget from 1433 to 405 MB, ran
  1024 MB staging buffers and sat over the VRAM budget for its whole parked stretch. It is
  consistent with the model, the finer levels do not fit a smaller budget, but it isolated nothing.

## BUG-016

The tile pool cannot move the overhead figure. The engine reports D3D12MA block bytes minus
allocation bytes as overhead, the pool is one fixed allocation counted as resource, and overhead
reads the same at 405 and 1024 MB. That also counts against the pipeline cache theory, pipeline
objects are not allocator blocks. Boot 1 settled the order question. The menu after ten minutes
at the Red Bull Ring as the first track read 104 MB, and the menu after three minutes at the
Nürburgring as the second read 360 MB. Recorded in
[BUG-016](../../bugs/BUG-016-vram-overhead-grows-across-scene-loads.md).

## Smaller findings

- **Natural log.** Every feedback shader takes `0.5 * ln` of the squared derivative where log2
  belongs, so every feedback texture asks for about 0.69 of the true level of detail, finer than it
  needs. Kunos's to fix, correcting it from here would make textures softer than they are today.
- **Variable rate shading.** The shaders only count pixels with `(SV_Position.x | y) & 15 == 0`, so
  coarse shading can starve the count entirely. Why VRS stopped the churn in
  [TODO-017](../../todos/TODO-017-tier-2-variable-rate-shading.md).
- **The streamer's own log lines exist.** They are written at info level to loggers created at
  warn. The exe applies `-log_info=<logger>` from the command line after that, which is expected
  to raise them, not tried.
- **Game logs start with NUL bytes.** Plain `grep` stops at "Binary file matches" and silently
  drops every later line, use `grep -a`.
