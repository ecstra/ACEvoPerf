---
name: BUG-016-vram-overhead-grows-across-scene-loads
kind: bug
description: the overhead part of the game's VRAM report climbs from 24 MB to 360 MB over an 80 minute session of repeated scene loads while resource memory stays identical per scene, which eats the headroom a 6 GB card needs and is the leading explanation for textures going slightly blurry after several reloads
updated: 2026-09-12
links: [directstorage-streaming, telemetry, BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache, BUG-010-texture-pool-shrinks-on-race-load-and-restart]
area: streaming
status: open
severity: medium
---

## Symptom

Owner, 2026-09-12: "Ive noticed sometimes after multiple reloads some textures become slightly
blurry, but cant test it now." No screenshot yet and no session named.

This is not BUG-010 coming back. That one was the tile pool being sized small during a scene
transition, and the fixed pool cured it. The pool still holds, see below.

## Measured

From the owner's own game log of 2026-09-11 (`log-260911-191750.txt`), one 80 minute session
on 0.9.1+release.6 with mod 0.3.1, thirteen scene loads: menu, Suzuka, menu, Suzuka, menu, an
online lobby, menu, Nurburgring, menu, Nurburgring, menu, Nurburgring, menu.

The game reports `QueryVideoMemoryInfo report: ... VRAM Used N MB (R MB resource + O MB
overhead + X MB other objects)` twice per load. Budget was 5226 MB throughout.

What holds:

- `[Tile Pool] sized to 1024 MB (16384 tiles)` appears once, at start, and never again.
- `[Mesh Streamer] canonical sizes forced -> mesh budget 1433 MB` once, at start.
- The four allocator pools (`meshStreamingPool` 32 MB blocks, `meshStaticPool` 64 MB blocks,
  the tiled instances pool, the tile pool) are created once at 19:17:53 and never recreated.
- Resource memory is identical every time the same scene loads. Nurburgring reports 3611 then
  3765 MB on all three of its loads, to the megabyte. The menu reports 2511 then 2552 MB on
  all seven of its loads. So texture residency is not decaying.

What grows, on the menu scene, which is the same content every time:

| time | overhead MB |
|---|---|
| 19:18 | 24 |
| 19:21 | 168 |
| 19:30 | 152 |
| 19:48 | 264 |
| 19:54 | 296 |
| 20:25 | 232 |
| 20:37 | 360 |

Peak usage in the session was 4665 MB of the 5226 MB budget, on the Nurburgring, leaving
561 MB. The same Nurburgring load costs 4463 to 4665 MB depending on when in the session it
happens, and the difference is the overhead line, not the resources.

Loading gets slower over the same session, on content that does not change. The menu scene is
26 meshes, 30 textures and 47 instance sets every single time:

| menu load | total | track resources streaming | commit meshes |
|---|---|---|---|
| 19:21 | 5.35 s | 1.86 s | 1.14 s |
| 19:48 | 6.92 s | 2.34 s | 1.36 s |
| 20:25 | 7.26 s | 2.70 s | 1.55 s |

36 percent slower by the third load. The Nurburgring does the same over its three loads,
16.92 s then 17.00 s then 17.68 s, with track resources streaming going 12.61 s, 12.64 s,
13.14 s for an identical 1528 meshes and 1237 textures. Whatever accumulates costs time as
well as memory, which makes an allocator that is doing more work to place each request a
better fit than a simple leak.

## Reading

Same scene, same resources, more overhead each time, and nothing in the pools is being rebuilt.
Something attributed to the process accumulates for the life of the run and is never released
on a scene unload.

The leading candidate is the pipeline state cache, the same flag as BUG-015. With
`enable_pso_cache=true` the game compiles and retains pipeline objects as it meets new track
and car combinations, the set only grows within a run, and it wrote a 31.3 MB
`pipeline.library` at exit from this very session. Driver side pipeline objects are much
larger than their serialised form, so a few hundred megabytes by the end is the right order.
That would make BUG-015 and this one the same cause with two symptoms.

Not proven. It could equally be descriptor heaps, command allocators or upload rings that grow
with the number of loads. The flag was turned off on 2026-09-12, which makes the test cheap.

There is no engine flag for the allocator worth reaching for: `dx12_amd_memory_allocator` and
`dx12_instances_tiled` are already on and wanted (TODO-002 measured the plain instances path
at 160 MB worse), and `dx12_amd_memory_allocator_within_budget` only makes the allocator crash
when a request exceeds the budget.

## First run with the cache off, 2026-09-12

A 26 minute session on the same machine with `enable_pso_cache=false`, four menu loads among
seven loads in all. The one way climb is gone, the figure now goes up and down:

| menu load | overhead MB | previous session, cache on |
|---|---|---|
| first | 24 then 103 | 24 then 103 |
| second | 360 then 297 | 168 then 169 |
| third | 104 then 104 | 152 then 149 |
| fourth | 200 then 169 | 264 then 233, and 360 by the seventh |

So it is no longer a ratchet, which is what the accumulation reading predicted, but one
reading still reached the same 360 MB peak. Peak use in the session was 4590 MB of the
5226 MB budget, against 4665 MB before, so the headroom has not moved much either.

Not settled. The shape changed in the direction the pipeline cache theory wants and the
magnitude did not. Two sessions of four menu loads each is too little to call it, and neither
was driven to the same script. What would settle it: the same route twice, once with the flag
on and once off, same tracks in the same order for the same minutes.

## Reproduce

Repeat the 2026-09-11 session shape with `enable_pso_cache=false`: menu, a track, menu, the
same track, menu, a heavier track, menu, and so on for about an hour and a dozen loads, then
read the `overhead` figures out of the game log in order. Two outcomes:

- Overhead stays low and flat: BUG-015's fix carried this one too, both close together.
- Overhead climbs the same way: the cause is elsewhere, and the next step is naming what the
  process holds, with the mod's timeline CSV on (`timeline=1`) so the climb is visible per
  second instead of twice per load.

Either way, the blurry texture report needs the owner to say which scene and after how many
loads, and a screenshot of a surface that looks wrong next to the same surface on a fresh
load.

## Ruled out: a newer D3D12 runtime, 2026-09-12

The overhead is runtime side allocation, so the Agility SDK was a fair suspect and got its own
session. The SDK itself was built for this and then removed from the mod once it measured to
nothing everywhere, see
[DEC-016](../decisions/DEC-016-agility-sdk-tried-and-removed.md), but the session stands:
28 minutes with D3D12Core 1.619.5 loaded and confirmed in the log,
`enable_pso_cache=false`, six scene loads, the owner's usual route of a Nürburgring lap, ten Red
Bull Ring hotlaps and a Touristenfahrten online lap.

| load | overhead MB |
|---|---|
| 1 menu | 24, 103 |
| 2 Nürburgring | 135, 120 |
| 3 menu | **328, 265** |
| 4 Red Bull Ring | 158, 86 |
| 5 menu | 104, 104, 121 |
| 6 Touristenfahrten online | 115, 111 |

Set against the two sessions already on file, all three at the same starting point of 24 then
103 on the first menu load:

| | second menu load | later menu loads | peak VRAM used |
|---|---|---|---|
| cache on | 168 | climbs to 360 and stays | 4665 MB |
| cache off | 360 | 104, then 200 | 4590 MB |
| cache off plus Agility 619 | 328 | 104, then 115 | 4568 MB |

Agility reproduces the cache off control in shape and within noise in magnitude. **A newer
runtime does not touch this bug**, which also makes the D3D12 runtime's own allocator a weaker
candidate than it was, because changing the whole runtime changed nothing.

What still holds from the cache off reading: the ratchet is gone and the spike is not. Whatever
produces a single 328 to 360 MB reading on one menu load and then releases it is still unnamed.

## Done when

The overhead figure is flat across a dozen loads, or its growth is named and the mod either
fixes it or the record says why it cannot.
