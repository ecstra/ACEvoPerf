---
name: session-leak-census-2026-09-16
kind: doc
description: TODO-023's census run, seven identical Red Bull Ring visits with two full memory dumps, the game's live heap growing about 57 MB a visit without levelling off, named from the dumps and the exe as every session left whole in memory by a cycle, the game mode a local server connection owns holding that connection in a list of strong pointers, with the heap slack, the VRAM side flat and the mesh budget never reached
updated: 2026-09-16
links: [BUG-016-vram-overhead-grows-across-scene-loads, TODO-023-name-what-the-game-keeps-across-identical-loads, memory-creep-2026-09-14, telemetry, TODO-022-frame-time-with-and-without-the-mod, BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash]
---

# A whole session left behind at every load

The run that [memory-creep-2026-09-14](memory-creep-2026-09-14.md) said would decide BUG-016. Session
`logs/census-rbr-20260916`, build `fix/memory-creep` (`ceb4988`, the memory census back plus BUG-022's fix),
`memory_census=1`, `timeline=1`, `frames=1`, `-log_info=meshStreamer` in the launch options. RVAs and offsets are
for the 0.9.1 exe.

The owner drove the route with two changes. The Red Bull Ring instead of the Nürburgring GP, and seven visits
instead of six. M1 was taken after the first return to the menu and M6 after the seventh, so six identical loads
sit between the two dumps. Each visit was a Time Attack practice with the Ferrari 296 GT3, parked in the pit box
for 47 to 119 s. Both dumps stay on the owner's disk outside the repo.

## The census

Settled menu readings, MB. In use is `HeapSummary`'s allocated count, which the dumps confirm to the MB.

| reading | commit | heap committed | heap in use | step in use |
|---|---|---|---|---|
| at start | 8161 | 3715 | 3598 | |
| after visit 1 (M1 follows) | 8748 | 4263 | 3724 | +126 |
| after visit 2 | 9080 | 4495 | 3795 | +71 |
| after visit 3 | 9210 | 4630 | 3854 | +59 |
| after visit 4 | 9294 | 4717 | 3912 | +58 |
| after visit 5 | 9357 | 4776 | 3969 | +57 |
| after visit 6 | 9426 | 4845 | 4030 | +61 |
| after visit 7 (M6 follows) | 9482 | 4904 | 4082 | +52 |

The live heap keeps growing on identical loads, about 57 MB a visit from the second on, with no sign of
levelling off and no link to how long the car sat in the pit box. `HeapCompact` returned 0 MB at every reading.

## What the dumps hold

`dumpheap.py` from the deep dive read the process heap (`0x1869AAF0000`, the one heap the census reports) in
under 10 s per dump. Its live total is 3724.3 MB at M1 and 4081.8 MB at M6, the census figures to the MB.

| growth M1 to M6 | MB | blocks |
|---|---|---|
| blocks the heap took straight from `VirtualAlloc` (1 MB and more) | +273.5 | +73 |
| backend blocks (16 KB to 1 MB) | +36.8 | +497 |
| low fragmentation heap slots | +47.2 | +636,081 |
| all live | +357.5 | |

Classes by RTTI, counts at M1 and M6. Six loads apart, so a class that gains six per visit gains 36.

| class | M1 | M6 | per visit |
|---|---|---|---|
| `Vector3Data` | 120,458 | 355,220 | 39,127 |
| `TransformData` | 21,634 | 72,256 | 8,437 |
| `SplineData_Node` | 5,738 | 39,386 | 5,608 |
| `ActorData` | 2,236 | 12,544 | 1,718 |
| `StaticMeshData` | 797 | 5,093 | 716 |
| `InstanceSetData` | 320 | 1,394 | 179 |
| `SeasonDefinition` | 9 | 33 | 4 |
| `SpawnPoint`, `ZoneData_Pitlane` | 4, 3 | 22, 21 | 3 |
| `_Ref_count_obj2<LocalServerConnection>`, `ConnectToServerCommand`, `CockpitCamera` | 3 | 15 | 2 |
| `TimeAttackRemote`, `DynamicWeatherService`, `RigidBodyODE`, `PenaltyInvestigations` | 1 | 7 | 1 |
| `PaintShopGameMode` | 2 | 8 | 1 |

The scene description of the track (actors, transforms, instance sets, splines, cameras, colliders, lights),
the season and session definitions, the weather service, a physics body and the game mode all pile up once or a
few times per visit, with the message handlers the game registers on a connection
(`_Func_impl_no_alloc<lambda, IRemote2LocalConnection&, const Message&>`, one to three per visit each).

**The big blocks are instance transforms.** 72 of the new direct blocks are four sizes, 5,900,156, 4,756,256,
3,124,652 and 2,095,232 B, 18 of each, three per visit, 45 MB a visit. Their contents are float triples in the
track's coordinates, and each is the repeated field of an `InstanceSetData` message (pointer at +0x18 of the
message into +0x8 of the block). So the track's parsed scene is kept three times per visit.

## Who keeps it

A reference cycle between each session's connection and its game mode. 14 of the 15 `LocalServerConnection`
control blocks in M6 have a use count of 1 and a weak count of 3, and the live one has 2 and 3, its extra owner
being `GameServerConnectionManager` at +0x30. Every pointer to a control block in the dump accounts for those
counts, read against the exe.

- **Object +0x100 is weak.** `LocalServerConnection` derives from `enable_shared_from_this`, and its make_shared
  (0x1245B00) fills that pair with a weak reference. The first reading of these dumps took it for a strong
  pointer to itself, and that was wrong.
- **The server's list is weak.** The connection embeds a `LocalGameServer` at +0x4B8 (constructor 0x1B24950,
  `ksPlatformCore\LocalGameServer.cpp`), whose list of connections at +0x130 is a `std::vector` of
  `std::weak_ptr`. Its destructor (0x1B24E90) only drops weak counts.
- **The game mode's list is strong.** The `LocalGameServer` owns the session's game mode at +0x160 and deletes it
  in its destructor (0x1B24FA0). `RemoteGameMode`, the base of `TimeAttackRemote` and `PaintShopGameMode`, keeps
  its connections at +0x3E8 in a `std::vector` of `std::shared_ptr`, released with use counts in its destructor
  (0x19150D1 into 0x1921B10). In M6 both lists hold the one connection, as a pointer to its second base at
  object +0x90.

So the connection owns its game mode and the game mode owns the connection. When the manager lets go at the end
of a session, the game mode's entry keeps the count at 1 and nothing can free either of them.

Two connections leak per visit, one for the practice session and one for the menu session the game returns to,
which matches one `TimeAttackRemote` and one `PaintShopGameMode` per visit. A connection keeps the parts below,
going by its pointer fields and by climbing the pointers back from leaked objects in M6.

- object +0x618, the session's game mode (`TimeAttackRemote` for the track)
- object +0x620, a 224 B block that both the connection and the game mode (+0x3E0) point at, which holds the
  session's state. Climbing from a leaked `InstanceSetData` reaches it through `InstancedStaticMeshData`,
  `ActorData`, a 2304 B actor list and a 792 B message. Climbing from a leaked `DynamicWeatherService` reaches it
  through `WeatherDataUpdate`.
- embedded commands and events of the handshake (`ConnectToServerCommand`, `StartLocalGamemodeCommand` with its
  `SeasonDefinition`, `GamemodeExitCommand`), a `ThreadsafeCommunicationChannel`, a `CommunicationChannelQueue`
  whose handler table holds the lambdas above, and a `shared_ptr<OnlineServices>`

A mark from every root in the style of a conservative garbage collector does not work on this heap. Stale values
in private memory and free space reach almost everything (4,018 of 4,082 MB), so it gives only a lower bound of
62 MB kept by the leaked connections alone. The climbs above, pointer by pointer, are the evidence.

## The slack

Committed memory of the heap against what is in use, MB.

| | committed | in use | not in use | backend free | free LFH slots | slot rounding | heap internal |
|---|---|---|---|---|---|---|---|
| M1 | 4087 | 3724 | 363 | 150 | 154 | 36 | 19 |
| M6 | 4782 | 4082 | 700 | 304 | 218 | 46 | 128 |

The real slack after one visit is 363 MB, and `HeapSummary`'s committed figure (4263 MB) overstated the real
commit by 176 MB. The slack grows with the leak, pinned pages among the leaked objects, and the heap internal
blocks grew 109 MB, which is not explained yet.

## The VRAM side is flat

- The game's report reads 2553 MB of resource, 232 MB of overhead and 651 to 700 MB of other objects at every
  menu, and 3789 to 3809 MB used at every track entry. The 24 to 232 MB overhead step comes once, with the first
  track, as the deep dive found.
- `d3d12ma_walk.py`, run on game dumps for the first time, finds the 32 MB block vector holding three blocks at
  both M1 and M6, 24.1 and 26.3 MB free, with block ids moving from 10 and 12 to 74 and 77. Blocks come and go
  with each load and do not accumulate at the Red Bull Ring.

## The mesh budget readout

`-log_info=meshStreamer` logged 105,124 lines of `tracked N, used N MB, budget 1433 MB`. Use peaked at 461 MB, so
nothing was trimmed at 1433 MB at the Red Bull Ring, the question TODO-022 asked of this run.

## What it costs and what could change it

About 57 MB of RAM and its share of page file for every track visit at the Red Bull Ring, the practice session and
the menu session after it, for the life of the process, and more on tracks with bigger scenes. Fifty visits would
hold about 3 GB.

The chain has one cause a DLL could reach, the game mode's strong entry. Ending that one reference once nothing
else holds the connection would let the whole session free. Its risk is that these destructors have never run in
the shipped game, since nothing ever freed a connection, so a fix has to be proven with the same census run
showing the heap flat and session exits and quits clean. The other path is a report to Kunos with these numbers.
The owner chose the fix, `[engine] session_leak_fix` on `fix/memory-creep`.

## Scripts

In the session's scratch space, not kept. `dumpheap.py`, `diffclasses.py` and `d3d12ma_walk.py` from the deep
dive, plus `rbr_diff.py` (the M1 to M6 diff), `owners.py` (references to a class or address), `fields.py`
(pointer fields of a block), `climb.py` (holders level by level), `paths.py` and `retained.py` (the conservative
mark).
