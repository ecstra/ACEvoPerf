---
name: memory-creep-2026-09-14
kind: doc
description: BUG-016's deep dive with no new run, the census read again with VRAM taken out shows a one time heap fill with the first track and then about 110 MB a track the game really keeps, the heap's own free count overstates, no heap lever from a DLL is worth shipping, the VRAM overhead spike is placement the next load reuses, and page file space is the only cost found
updated: 2026-09-14
links: [BUG-016-vram-overhead-grows-across-scene-loads, TODO-023-name-what-the-game-keeps-across-identical-loads, BUG-023-overlay-keeps-a-64-mb-table-copy-for-the-whole-session, BUG-019-car-physics-rebuilds-every-tyre-model-five-times, directstorage-streaming, telemetry, DEC-003-staging-buffer-128mb, DEC-005-fixed-pool-sizes-by-default, package-override-layer]
---

# Memory creep, the deep dive

BUG-016's round of 2026-09-14. Five angles worked in parallel from the exe, the sessions on disk and
the history, each checked by a second agent, then a critic and three gap rounds. No new session was
run. RVAs are for the 0.9.1 exe. S is `logs/memcreep-20260913/S-census-settled` (mod on, memory
census), Q is `Q-passive` (mod passive, no census) and R is `R-census-mod-on`. The agents' scripts
and test programs lived in the session's scratch space and are not kept.

## How the process allocates

- **One heap serves the whole process.** The exe imports `malloc`, `free`, `realloc`, `calloc`,
  `_aligned_malloc` and `_aligned_free` from the UCRT, whose heap handle is the process heap, and it
  never calls `HeapCreate`. Its `operator new` at `0x2e331e0` has 28,166 direct call sites. Cohtml's
  allocator forwards to the exe's CRT, and fmod, the DirectStorage core, the Steam API, DLSS, D3D12
  and DXGI allocate from the process heap as well.
- **It is an NT heap with the low fragmentation front end.** The manifest asks for no segment heap,
  there is no Image File Execution Options entry, and the load config leaves the decommit thresholds
  at zero.
- **The engine keeps no CPU memory tracker.** Its only memory report (`0x27c7c10`) calls
  `K32GetProcessMemoryInfo`. The only clear paths found by string are the texture cache, a thumbnail
  cache and the PSO cache.

## The census read again

The census of 2026-09-13 counted every heap with `HeapSummary` and walked the address space. Two
things were missing from how it was read.

**Commit counts VRAM.** The game's private commit includes its local VRAM one to one. The tile pool
alone moves commit by what it moves VRAM (P2's 366 MB pool against M's 1024 MB, resource minus
658 MB and commit minus 662 to 667 MB), and in S the commit charge minus the census walk stays at 143
to 155 MB while VRAM swings by hundreds. So BUG-016's "other private" column held 3.2 to 3.5 GB of
VRAM.

The settled menu readings of S with VRAM taken out, in MB:

| menu reading | commit | live heap | VRAM | heap slack and other private |
|---|---|---|---|---|
| at start | 8149 | 3569 | 3236 | 1179 |
| after the Nürburgring GP | 9401 | 3870 | 3498 | 1825 |
| after the Red Bull Ring | 9428 | 3938 | 3354 | 1938 |
| after the Nürburgring GP | 9578 | 4104 | 3472 | 1797 |
| after the Red Bull Ring | 9731 | 4157 | 3362 | 2011 |

- **The first track is a one time step.** 1,252 MB of commit at the settled menu, of which 262 MB is
  VRAM, 301 MB live heap, 32 MB `VirtualAlloc`, 11 MB outside the walk and 646 MB heap slack or
  other private memory.
- **After that the growth is live.** Between two visits to the same track the Nürburgring GP menu
  gained 234 MB of live heap while slack and other private memory moved by minus 28 MB, and the Red
  Bull Ring menu gained 219 MB while they moved by plus 73 MB. So the creep that keeps going, about
  110 to 117 MB a track in S, is memory the game holds, a leak or a cache, and not fragmentation
  getting worse.
- **Only same track pairs are safe to quote.** Single steps after the first track swing by about
  150 MB with whichever track came last.

**The heap's own free count overstates.** `HeapSummary`'s committed figure drifts above the real
commit after load shaped churn, and compaction does not pull it back while survivors live. A test
program on this machine shaped like S's route overstated real commit by 278 to 533 MB after every
unload, and compacting moved the count by 10 to 41 MB. In the game compaction moved it by 1 to 18 MB.
The 1,767 MB "given back" in BUG-016 is that count. The real slack is most likely 0.74 to 0.96 GB,
set once by the first track, with a loose upper bound of about 1.8 to 2.0 GB that only a walk of the
heap's blocks can close.

## Why freed memory stays committed

Reproduced with a test program on this machine's `ntdll`. Large and medium blocks go back at free
time even with survivors between them. Small blocks do not. 649 MB of small blocks with 5 percent kept
at random still hold 677 MB, while the same 5 percent kept clustered holds 79 MB. One scattered small
survivor keeps 210 to 360 KB committed, and 32 KB blocks do not pin at all. A later load of the same
shape reuses part of the held space.

What a DLL loaded at process start can do about it, each tried on the test program:

| lever | result |
|---|---|
| `HeapCompact` at a scene change | returns nothing, in every NT heap case and at all five game readings |
| `HeapSetInformation` with `HeapOptimizeResources` | gives back 6 to 7 percent in 2 to 6 ms, about 45 to 70 MB here, and the next load takes it again |
| the exe's CRT imports moved to a private segment heap | 30 to 65 percent less held, only once compacted since a segment heap keeps freed small pages until then, and a free of a block from the old heap ends the process with `0xC0000374` while `HeapValidate` cannot tell the two heaps apart |
| a segment heap for the whole process | needs an administrator registry write that a zip install cannot make |
| the decommit thresholds | fixed when the heap is created, and already eager |
| turning the low fragmentation front end off | cannot be done once it is on |
| a working set trim | moves RAM only, commit stays |
| a replacement allocator such as mimalloc | a new dependency with the same foreign free crash |

None of them touches the live growth, and none is worth shipping.

## The VRAM overhead spike

- **What the figure is.** The engine's budget report at `0x1e37f20` writes resource as the D3D12
  memory allocator's local allocation bytes, overhead as block bytes minus allocation bytes, and other
  objects as usage minus block bytes.
- **Where the spike sits.** `meshStreamingPool` is created with 32 MB blocks at `0x1c72037` for the
  mesh buckets, with no block limit and no call to defragmentation. The first menu report after a
  track reads 40 plus 32 k MB of overhead in 35 of 38 loads, 24 MB on a fresh boot, with k at 0 to 4
  after the Red Bull Ring, 6 to 8 after the Nürburgring GP and 7 to 10 after the 24h layout. The menu
  scene starts loading while the track's buffers are still being freed (S at 17:21:19.723, VRAM still
  falling), so menu buffers land in the track's blocks and pin them.
- **It is not a leak.** Every later menu reads 2511 MB of resource and every track reads the same
  overhead on its first report on every visit. The allocator keeps at most one empty block per vector
  (`BlockVector::Free` at `0x1fc3610`), and the next load fills the pinned blocks. AI cars hold none
  (a 29 AI race on the 24h layout left k at 8, inside the solo range).
- **Its one cost is already gone.** The engine's dynamic pool sizing (`0x1c880f0`) subtracted usage
  including block bytes, so on 0.9.0 a settings apply after the Nürburgring cut the tile pool from 633
  to 526 MB. `force_canonical_pool_sizes` skips that path (the branch at `0x1c80cd7`).

Which allocations sit in the extra blocks was not confirmed.

## What the creep costs

- **Page file space, and nothing else found.** S grew 1,580 MB of settled commit over four tracks and
  Q 1,827 MB over three, about 4 percent of the machine's 44 GB of system commit. 50 of 67 sessions
  went past the commit limit they started with and the page file grew. The 29 AI races peak at 17.6
  to 19.9 GB committed. No out of memory line appears in any game log.
- **Not load time.** The menu load after a Nürburgring takes a median 4.53 s against 2.59 s after a
  Red Bull Ring because it contains the Nürburgring's teardown, 1.46 s against 0.20 s. Identical loads
  after identical predecessors drift a median 4 to 7 percent with no link to commit. BUG-016's table
  of slowing menu loads mixed predecessors, and its log is no longer on disk.
- **Not blur.** Repeat visits load the same resource bytes (S Nürburgring GP 3476 then 3600 MB on
  both visits, Red Bull Ring 2849 then 2956 MB), and on S's two Nürburgring visits the texture streamer
  turned away 4847 and 4830 loads a minute and used a median 15,219 and 15,254 tiles. The owner's
  "slightly blurry after many reloads" fits BUG-020 and BUG-021 better.
- **Not frame time.** Repeat visits run minus 3.8 to plus 2.1 percent in median frame time.
- **The steady VRAM climb was 0.9.0 with the PSO cache on.** On 0.9.1 VRAM other objects rise 110 to
  125 MB with the first track in every session, passive too, then move 0 to 21 MB per menu load.

## The mod's share

- **A fixed 66 to 71 MB, nothing that grows.** 64 MB of it is the overlay's copy of the package table
  (`g_toc` in `src/overlay/overlay.cpp`), only read while the game reads its table, which in S ended
  within 2 s of attach, filed as
  [BUG-023](../../bugs/BUG-023-overlay-keeps-a-64-mb-table-copy-for-the-whole-session.md). The trace
  maps stay under 10 MB.
- **The same step as passive.** With the game's own pool sizes the mod's first track step matches the
  passive run to 3 MB (P1 plus 1,359 against Q plus 1,356).
- **The staging buffers.** Commit carries four times the staging size, two buffers in VRAM and two on
  the CPU side, so the 128 MB cap takes 2.9 GB off the first menu. The CPU side
  `SystemMemoryStagingBuffer` comes from the core's static CRT `_aligned_malloc` on the process heap
  (`acevo_dstoragecore.dll` 0x12452 into 0x3a770), and the buffers are kept per factory and device,
  not per queue.

## What would decide it

The data on disk cannot tell a one time fill from a steady leak. No session has more than four track
visits, commit alone hides live growth inside the held slack, and the same repeat step differs by 150
to 300 MB between runs. One run can, filed as
[TODO-023](../../todos/TODO-023-name-what-the-game-keeps-across-identical-loads.md).

- The memory census back in a developer build exactly as S ran it, the shipped ini otherwise.
- The Nürburgring GP six times, parked 60 s each, back to the menu each time.
- A Task Manager full memory dump at the first and the sixth menu.

The census steps say whether live heap keeps growing on identical loads. The two dumps name what grew
by C++ class and block size, measure the real slack, and walk the mesh pool's blocks. A Task Manager
dump needs no administrator and no build, and a reader rebuilt a test heap from such a dump to the
byte, every busy block found, busy bytes equal to `HeapSummary`'s allocated count and committed equal
to `VirtualQuery`. For the game a dump is 11 to 12 GB and freezes it for an estimated 15 s to a
minute. It holds the whole process memory, so it stays on the owner's disk and is never shared.

Live growth that stops after the second visit closes the RAM side as the game's own fill. Steady live
growth gets named from the dumps, and only two kinds of owner open a fix the mod could ship, a per
load registry the engine could release at unload, or a history container nothing reads back. Anything
else is a report to Kunos with the numbers.

## Smaller findings

- **BUG-019's tyre builds are not where the time goes.** Each build appends a 1,096 byte record to
  its wheel and its logged part takes about 0.15 ms, 12 ms for 80 builds in S. The time sits in the
  5.6 s of gaps between builds, around the compound asset fetch at `0x105fd00`. The Ferrari's preset
  lists five compounds per axle, three of them byte identical.
- **Mapped views jump in the first menu.** The gigabyte BUG-016 put on the first track appears in R
  between 10.5 and 20.8 s, before any track. The cause is open and may be the census's own first
  reading. Mapped memory is not commit.
- **UI views per load.** S registers two new Cohtml views at every scene load and logs unloading only
  for view 0.
- **Old crash dumps.** Three 0.9.0 Windows Error Reporting dumps (two UCRT fast fails on Resource
  Manager Worker 0, one access violation in `v8.dll`) had 10.6 to 11.4 GB of commit headroom, so memory
  did not end them.
