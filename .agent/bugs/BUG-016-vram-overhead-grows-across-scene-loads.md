---
name: BUG-016-vram-overhead-grows-across-scene-loads
kind: bug
description: the game's committed memory grows across scene loads, the same with the mod passive, a one time heap fill with the first track and then a real leak, named by the census run as every session staying in memory behind a LocalServerConnection that holds a shared pointer to itself, about 57 MB a Red Bull Ring visit, while the VRAM side is placement the next load reuses
updated: 2026-09-16
links: [memory-creep-2026-09-14, session-leak-census-2026-09-16, TODO-023-name-what-the-game-keeps-across-identical-loads, BUG-023-overlay-keeps-a-64-mb-table-copy-for-the-whole-session, directstorage-streaming, telemetry, BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache, BUG-010-texture-pool-shrinks-on-race-load-and-restart, texture-streamer-flip-2026-09-13]
area: streaming
status: open
severity: bug
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

**Corrected 2026-09-14.** The menu loads in that table followed different tracks. A menu load
after a Nürburgring carries its teardown (a median 4.53 s against 2.59 s after a Red Bull Ring), and
identical loads after identical predecessors drift a median 4 to 7 percent with no link to commit,
see [memory-creep-2026-09-14](../docs/research/memory-creep-2026-09-14.md).

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

## The spike follows the Nürburgring, 2026-09-13

**What the figure measures.** With `dx12_amd_memory_allocator` on, the engine's report reads the
D3D12MA budget (`0x1e37f20`). Resource is allocation bytes, overhead is block bytes minus allocation
bytes, the free space inside the allocator's blocks, and other objects is the rest of the process's
usage. So the tile pool cannot be the cause. It is one fixed allocation counted under resource,
created once per session, and overhead reads the same with a 405 MB pool as with 1024 MB. The same
definition counts against the pipeline cache theory, pipeline objects are not allocator blocks.

**The order test.** Every earlier session loaded the Nürburgring first, so the spike could have been
the first track unload of the process. Boot 1 of TODO-018's live round (`logs/streamer-boot1-1124`,
game log `log-260913-110718.txt`) reversed the order.

| menu load | after | stint | overhead MB |
|---|---|---|---|
| first | start | none | 24 then 103 |
| second | Red Bull Ring, first track | 10 minutes | 104 then 104, 121 |
| third | Nürburgring, second track | 3 minutes | **360 then 297** |

The spike follows the Nürburgring's content, whatever order it loads in and however long it ran.
The next step is naming what the Nürburgring leaves in the allocator's blocks when it unloads.
Full context in [texture-streamer-flip-2026-09-13](../docs/research/texture-streamer-flip-2026-09-13.md).

Game logs start with one or two NUL bytes, so plain `grep` reports "Binary file matches" and drops
every later line. Readings taken from these logs need `grep -a`.

## The creep is in RAM too, and it is the game's, 2026-09-13

Owner wording, 2026-09-13: "Frametime, Memory Creep (leak), 1% all still remain and they're bigger."

**RAM grows more than VRAM.** The game logs its own committed memory at every scene change,
`Used Memory (MB): commit size N (peak P) - working set W`. The last reading before leaving the
menu, which is the same content every time, from every multi track session on disk.

| session | first menu | after each track, in order |
|---|---|---|
| `agility-long-on-2042`, mod on | 8340 | Nürburgring 9377, Red Bull Ring 9621 |
| `streamer-boot1-1124`, mod on | 8141 | Red Bull Ring 8735, Nürburgring 9502 |
| `frametime-20260913/P3-meshes-366`, mod on | 8342 | Nürburgring GP 9317, Nordschleife 9454, Red Bull Ring multiplayer 9542, Nordschleife 29 AI race 10006 |
| `memcreep-20260913/Q-passive`, mod passive | 11106 | Nürburgring 12147, Red Bull Ring 12410, Nürburgring again 12876 |

The first track adds about a gigabyte that the menu keeps, every later track adds 90 to 470 MB,
and the second visit to the same track still adds 466 MB, so it is not only a cache that fills
once. The 29 AI Nordschleife race peaked at 19,887 MB committed with 10,680 MB resident.

**The mod passive shows the same growth.** Run Q used the settings of the passive run N on the
route menu, Nürburgring GP, menu, Red Bull Ring, menu, Nürburgring GP, menu, Red Bull Ring, one
lap each. Its steps, +1041 and +263 MB for the Nürburgring then the Red Bull Ring, match the
mod on session of 2026-09-12 on the same two tracks, +1037 and +244 MB. The VRAM side matches as
well: other objects rise about 110 MB with the first track and a few MB per load after that, and
the overhead spike still follows each Nürburgring unload, 265 and 233 MB. So the creep is the
game's own.

**The mod already takes 2.9 GB off.** The first menu commits 11,052 MB in the passive run N and
8,198 MB in run M, the difference being the game's 1024 MB staging buffers that the mod cuts to
128 MB.

Run Q ended in a GPU hang on its last Red Bull Ring lap, `Device removed, reason:
DXGI_ERROR_DEVICE_HUNG`, with the NVIDIA driver's own timeout in the Windows event log at the
second the frames stopped and no driver error in the fourteen days before. The owner had
undervolted the card that day and took the undervolt off again. Committed memory was 13,652 MB of
32 GB and VRAM 4,924 of 5,226 MB when it hung, so memory was not the trigger.

Next is naming what the committed memory is made of and which code keeps it, which the game does
not log. Its only breakdown is the VRAM bucket list it prints after a GPU crash.

## What the growth is made of, 2026-09-13

A developer memory census read the process once memory had settled in the menu. It remembered who
committed memory through the `VirtualAlloc` imports of every module, read every heap with
`HeapSummary`, walked the address space, and then compacted the heaps with `HeapCompact` and read
again. It was built for this round and taken out again with the deep dive deferred, commit `0770d67`
holds the last version and `8450d2e` removed it. Session `logs/memcreep-20260913/S-census-settled`,
mod on, the route of run Q, every figure in MB.

| menu reading | commit charge | heap committed | heap in use | VirtualAlloc imports | other private | mapped |
|---|---|---|---|---|---|---|
| at start | 8149 | 3692 | 3569 | 22 | 4292 | 348 |
| after the Nürburgring GP | 9401 | 5325 | 3870 | 54 | 3869 | 1372 |
| after the Red Bull Ring | 9428 | 5334 | 3938 | 45 | 3896 | 1372 |
| after the Nürburgring GP | 9578 | 5903 | 4104 | 50 | 3470 | 1372 |
| after the Red Bull Ring | 9731 | 5924 | 4157 | 46 | 3607 | 1372 |

- **It is the game's main heap.** One heap holds all of it, the one the CRT's `malloc` uses. Memory
  committed through `VirtualAlloc` imports stays near 50 MB, and the rest of private memory falls
  rather than grows.
- **The game really uses only 588 MB more** after four tracks, 301 MB of it after the first.
- **The heap holds 1.8 GB it has been given back**, 123 MB at start and 1,767 MB by the end, by the
  heap's own count. `HeapCompact` on every heap returned nothing from the commit charge at any of the
  five readings, so that free memory is spread across pages still partly in use. A harness showed
  the heap's committed figure can lag the real charge, so the split between in use and free is the
  heap's accounting, while the commit charge and the zero returned are exact.
- **Mapped views jump by a gigabyte with the first track** and stay flat after it.

Reading this: the creep is a small real growth in what the game keeps, about 300 MB with the first
track and 50 to 170 MB with each track after, plus fragmentation of the process heap that no call
into the heap undoes. The mod cannot defragment a heap the game is using, and moving the game onto a
different allocator from a DLL would have to catch every allocation and every free from the first one
on.

**Corrected 2026-09-14.** Two readings above do not hold. The game's commit counts its local VRAM one
to one, so the other private column holds 3.2 to 3.5 GB of VRAM. And `HeapSummary`'s committed figure
overstates after load shaped churn, so the 1.8 GB "given back" is mostly that count. With VRAM taken
out the first track is a one time step of 1,252 MB, and after it the growth is live heap, 234 MB and
219 MB between visits to the same track while slack moved by minus 28 and plus 73 MB. The real slack is
most likely 0.74 to 0.96 GB, set once. The mapped views jump in the first menu before any track, not
with the first track.

The first census version read every ten seconds and froze the game on each read, 200 ms in the menu
and 1.4 s on track (`logs/memcreep-20260913/R-census-mod-on`), because `HeapSummary` walks a heap of
4 to 7 GB under its lock.

Deferred on the owner's word, 2026-09-13, to its own deep dive with a targeted fix if one exists.

## The deep dive, 2026-09-14

Five angles from the exe and the sessions on disk, no new run. The full record is
[memory-creep-2026-09-14](../docs/research/memory-creep-2026-09-14.md).

- **RAM.** One process heap serves the exe, Cohtml, fmod, DirectStorage, D3D12 and the driver. The
  first track fills it once, about 650 MB of slack, and after that the game keeps about 110 MB a track
  for real. No heap lever a DLL can pull is worth shipping, `HeapCompact` returns nothing and
  `HeapOptimizeResources` gives back 6 to 7 percent that the next load takes again.
- **VRAM.** The overhead spike is free space in the mesh pool's 32 MB blocks, pinned by menu buffers
  placed while the track was still unloading. The next load reuses it, and its only cost was the
  engine's dynamic pool sizing, which `force_canonical_pool_sizes` already skips.
- **Cost.** Page file space. No blur, frame time, load time or out of memory evidence on any session.
- **The mod.** A fixed 66 to 71 MB, 64 MB of it the overlay's table copy,
  [BUG-023](BUG-023-overlay-keeps-a-64-mb-table-copy-for-the-whole-session.md).

What keeps growing is still unnamed. One run with the census back and two memory dumps decides it,
[TODO-023](../todos/TODO-023-name-what-the-game-keeps-across-identical-loads.md).

## The census run names it, 2026-09-16

TODO-023's run, seven identical Red Bull Ring visits with the census and two full memory dumps. The
full record is [session-leak-census-2026-09-16](../docs/research/session-leak-census-2026-09-16.md).

- **It is a leak.** The live heap grew 126 MB with the first visit and then 52 to 71 MB with every
  identical visit, about 57 MB, with no sign of levelling off.
- **Every session stays in memory.** Between the dumps, six visits apart, each visit left its practice
  session and the menu session after it, with the track's parsed scene three times over (45 MB of it the
  transform arrays of the four biggest instance sets), the season and session definitions, the game mode,
  the weather service, a physics body and the handlers registered on the connection.
- **The owner is a connection that owns itself.** Each `LocalServerConnection` holds a `std::shared_ptr`
  to itself at object +0x100. 14 of the 15 in the last dump have that as their only owner, and the one
  in use has `GameServerConnectionManager` as its second. The connection points at its game mode (+0x618)
  and at the block that reaches the session's scene and weather (+0x620).
- **The slack.** 363 MB of the heap's committed memory was not in use after one visit and 700 MB after
  seven, and `HeapSummary` overstated the commit by 176 MB.
- **VRAM stays flat.** The same resource and overhead at every menu, and the mesh pool's 32 MB blocks at
  three in both dumps.

A fix the mod could try is resetting a released connection's pointer to itself, so the whole session
frees. Nothing has ever freed one in the shipped game, so those destructors are untested, and it needs
the same census run to show the heap flat and exits clean. The other path is a report to Kunos.

## Done when

TODO-023's run says whether live heap keeps growing on identical loads, and either the growth is
named and the mod fixes it, or the record says it is the game's own fill or cache and why the mod
cannot reach it.
