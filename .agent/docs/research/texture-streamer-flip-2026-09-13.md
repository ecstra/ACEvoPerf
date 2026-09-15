---
name: texture-streamer-flip-2026-09-13
kind: doc
description: the tile churn is the texture streamer measuring detail against the mip it has loaded and reading the answer as if against the full texture, confirmed live, fixed from the mod behind an off by default switch, with the pool found full in normal play, 33.6 MB/s of repeat traffic while driving at the Red Bull Ring and 3.5 GB of repeated file reads a session
updated: 2026-09-14
links: [TODO-018-look-properly-at-the-streaming-layer, TODO-019-tile-upload-dedupe-done-properly, TODO-021-the-engine-reads-the-same-data-twice, DEC-017-streamer-reload-fix-refuses-the-drop, tile-pool-reshuffle-2026-09-12, directstorage-streaming, telemetry, BUG-016-vram-overhead-grows-across-scene-loads, TODO-017-tier-2-variable-rate-shading, mesh-level-of-detail-2026-09-14, texture-streamer-camera-cuts-2026-09-14]
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
  pending tiles exceed the pool, true in code (`0x1F25F1B`) but never seen in six traces. The kicks
  that are forced come from camera cuts and from the UI waiting for textures
  ([texture-streamer-camera-cuts-2026-09-14](texture-streamer-camera-cuts-2026-09-14.md)).
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

Boot 1 and boot 2 ran at different thermal states, so neither says anything about frame rate.

## What it is worth in frames

A controlled pair on 2026-09-13 (`logs/perf-ab-20260913`), after the owner undervolted the GPU and
set the fans aggressively. Same build, same Nürburgring preset and pit exit spot, the GPU cooled back
to 47 and 49 °C before each launch, `nvidia-smi` sampling throughout. Run A had the fix and the
trace off, so no hook was installed at all. Run B had the fix on. Parked windows are the same 100 s
of each stay, chosen from the per second tile traffic.

| | run A, off | run B, fix on |
|---|---|---|
| parked fps | 88.42 | **89.66** |
| parked median, p99 frame time | 11.27, 14.04 ms | 11.13, 13.81 ms |
| parked 1 percent low | 68.9 | 69.5 |
| parked tile traffic | 19.25 MB/s | **0.00 MB/s** |
| parked process CPU | 26.7 percent | 25.4 percent |
| parked GPU clock, power, temperature | 1822 MHz, 94.4 W, 70.6 °C | 1822 MHz, 95.5 W, 71.8 °C |
| lap fps, 1 percent low | 86.31, 63.9 | 86.95, 63.8 |
| lap tile traffic | 14.88 MB/s | 13.51 MB/s |

The GPU sat at the same clock with no throttle reason active in either run, the first pair on this
machine where the card was not the variable. **Parked the fix gains 1.23 fps, 1.4 percent**, with a
95 percent interval of 1.02 to 1.43 fps over the per second samples, which covers the noise inside
the two runs and not a difference between sessions. Within run A the frames that carried a tile
request averaged 12.37 ms against 11.30 ms for the rest, but they are only 115 of 8,842, so most of
the cost is spread across every frame, the uploads and remaps competing with rendering on a GPU at
97 percent. The laps differed by seven seconds of pace and their difference is inside lap noise.

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

## Was the reload there for a reason

The owner's question before keeping the fix: "what if it was there for a reason? ACE has something
called dyanmic track and what if that + multiplayer needed that to see if a tile changed". Checked
three ways.

- **What a reload carries.** Across boot 1 and boot 2, 20,081 times a texture's mip was requested
  again, every one with the same package offset and size, so the same bytes from a read only file.
  The other 1,040 repeats of a resource address were a different texture that took a freed
  address.
- **Where the dynamic track lives.** The engine creates `dynamicTrackBuffer`, 65,536 entries of 4
  bytes (`0x1da2a31`), and the dynamic track materials read it through grid constants
  (`dynamicTrackTileDx`, `dynamicTrackGridLenghtX`). The streamed textures the fix held include 7
  shared surfaces those materials also use, concrete, grass and the GP base, 24 of 1,401 refusals in
  boot 2, all static layers.
- **Whether anything writes into a streamed texture.** `src/render/texture_writes.cpp` records each
  streamed texture's creation flags and hooks the four command list copy calls on all three command
  list vtables, which are separate in this runtime (a first run hooked only the direct one and saw
  no DirectStorage uploads at all). Over a thirty car race with skid marks
  (`logs/ai30-A-fix-on`) there were 1,905 streamed textures, none created writable by a shader,
  19,750 copies into them by DirectStorage, and 0 by the game or anything else. A multiplayer,
  practice, rain and ten car session before it showed the same with the direct list only. The owner
  saw skid marks, dirt, rain and cars behave normally throughout.

Nothing the game does at runtime changes a streamed texture, so there was nothing for the reload to
refresh.

## Thirty cars

With 29 AI the pool sat at its ceiling the whole race and track and car textures were pushed to their
coarsest level in bursts, which the owner saw as blurry cars at the start and when the field passed,
recorded in [BUG-020](../../bugs/BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles.md),
whose causes are in [texture-streamer-overload-2026-09-13](texture-streamer-overload-2026-09-13.md).
The same race with the fix off (`logs/ai30-B-fix-off`) looked the same to the owner, "blur looked
the same. I dont think this is related to our streaming fix, this is overload issue". Over the racing
time of each run the tile queue carried 1,020 MB a minute with the fix and 1,425 without, and loads
turned away for space were 5,844 and 5,237 a minute. The two races differed in length and in what
happened in them, so those are indications rather than a controlled pair.

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
  warn. The level is applied when each logger is created, from name lists built at logging init, so
  `-log_info=<logger>` is expected to raise them, traced for `meshStreamer` on 2026-09-14
  ([mesh-level-of-detail-2026-09-14](mesh-level-of-detail-2026-09-14.md)) and not tried.
- **Game logs start with NUL bytes.** Plain `grep` stops at "Binary file matches" and silently
  drops every later line, use `grep -a`.
