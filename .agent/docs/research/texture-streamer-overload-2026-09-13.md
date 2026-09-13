---
name: texture-streamer-overload-2026-09-13
kind: doc
description: a census of every kick in a thirty car race shows the texture streamer ranking each texture by its least important request and loading only whole steps, both fixed from the mod and seen fixed in game, with what is left being a 1 GB pool that holds the player's car and driver at the top rank and ranks AI cars like trackside props, left that way on the owner's call
updated: 2026-09-13
links: [BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles, DEC-018-a-full-texture-pool-keeps-the-players-car-first, texture-streamer-flip-2026-09-13, directstorage-streaming, telemetry, DEC-017-streamer-reload-fix-refuses-the-drop, BUG-016-vram-overhead-grows-across-scene-loads, BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash]
---

# The texture streamer with a full pool

BUG-020's deep dive. Two races of 29 AI at the Nürburgring GP on the reference laptop, the same car
and the same plan each time. Sessions `logs/census-ai30-20260913`, the census with no new fix, and
`logs/fix1-ai30-20260913`, with both fixes below. RVAs are for the 0.9.1 exe.

## The census

`[developer] streamer_census` copied what each kick ranked into `acevo_perf_census.bin`, every
demand entry, every record the engine built with its priority and whether it was admitted, and
every eighth kick what each tracked texture held. The first race wrote 1,181 kicks, 235 MB. It came
in with commit `a2bfc5e`, where the source and the file format are, and went out again with commit
`removed: the streamer census, BUG-020 fixed`. The readers used here lived in the session's scratch
space and are not kept.

## How a kick ranks demand

Every object that draws a texture pushes a request carrying flags and a distance. The kick swaps
that distance for a stand in from the object's visibility LOD, 0, 50, 200, 500, 1000, 2000, 4000
or 8000 (`0x343b858`), and an object not found in visibility keeps 65000. Each request becomes one
record per level of the texture, priced by the first table its flags pick, all set in the streamer
constructor at `0x1f1dd27`.

| flags | table | priority at level l |
|---|---|---|
| 0x4, the car the player drives and its driver | focus | 60000 minus 500 l, at any distance |
| 0x10, vehicle material category | vehicle | 40000 over 500 m, minus 200 l |
| 0x8, interior material category | interior | 8000 over 80 m, minus 500 l |
| 0x2000 | | 25000 over 150 m, minus 250 l |
| 0x4000 | | 10000 over 100 m, minus 600 l |
| no category, levels the GPU feedback asks for | feedback | 25000 plus half the sample count up to 40000, minus 100 l |
| 0x8000, otherwise | | 35000 over 300 m, minus 150 l |
| otherwise | default | 15000 over 200 m, minus 300 l |

A table falls to zero at the end of its range and a zero priority builds no record. The 0x4 flag
is bit 6 of the drawn object (`0x1dc32bd`, `0x1dc3706`), and in both races it sat only on the
player's Ferrari 296 GT3, the shared car assets it uses and the driver. No material in either race
carried the vehicle category, so the AI cars, a BMW M4 GT3 and several Audis among them, were ranked
on the default table like any trackside prop.

## Who holds the pool

Lap 5 of the second race, 97 kicks while the field passed, from the tracked texture lists.

| holder | tiles | MB |
|---|---|---|
| the player's car and the car assets it uses | 6,069 | 379 |
| the player's driver | 1,732 | 108 |
| track textures admitted | 3,575 | 223 |
| tiles the streamer does not track | 1,204 | 75 |
| coarsest levels of textures not admitted, 628 tiles of them the AI BMWs | about 2,650 | 166 |
| the margin the load gate keeps free | 1,024 | 64 |

The pool sat at 15,360 of 16,384 tiles, capacity minus the margin, for both races, and VRAM ran at
5,131 to 5,229 MB through the first race against a 5,226 MB budget, so there was no room to give
the pool more.

## A texture ranked by its least important request

The kick keeps one record per texture level. At `0x1f2e0f4` it sorts the records with a comparator
that orders a texture level's records by ascending priority, and the unique pass at
`0x1f2e108..0x1f2e168` keeps the first of each. The lowest request wins.

The player's livery shows it. On the grid its requests were the player's car at 60000 and AI
Ferraris of the same model at LOD 1, 11250. The records came out at 11250 to 10050, below an edge
of 13800 to 14100, and the livery was dropped to its coarsest level at the spawn. Wherever no AI
Ferrari was within LOD 1 the same texture ranked 60000 again. Across the whole first race every one
of 7,036 records whose texture had differing table requests carried the lowest of them and none the
highest, and the 355 where feedback took part carried the lowest too.

Ranked again offline with each level keeping its highest request, against the same budgets, the
player's Ferrari went from 77 to 170 MB admitted to 173 MB in every window, the grass ground
`grass_2` from 2 to 8 MB to 19 to 25 MB of the 31 MB asked, and `curb2` to everything it asked.
On the grid the track lost 265 MB, all of it textures no feedback had ever measured on screen,
trees, glass, refuellers and screen frames, while the GP base texture and the grass ground gained.

## Loads that are all or nothing

The load step at `0x1f212f1` sums the tiles of every level between the current and the admitted one
and starts nothing unless the whole sum fits the gate, avail minus the tiles already started this
kick. The skip at `0x1f2137b` still counts the attempt against the kick's quota of 128 at
`Streamer+0xE4`.

The livery wanted 340 tiles to go from its 256 by 256 tail to 4096 and waited 72 kicks after the
race start, from kick 167 to 238. The gate at its turn was 0 to 216 tiles on most of those kicks,
enough for the three levels under the finest, 84 tiles, and it loaded when a kick left it 728. The
grass ground asked for 2 tiles to leave its coarsest level and was turned away kick after kick,
often on kicks where all 128 attempts had been turned away before the quota ran out.

## The fixes

Both in `src/engine/streamer.cpp`, where the source comment has the detail.

- `streamer_rank_fix` moves the one call at `0x1f2e0f4` to a sort that orders a texture level's
  records by descending priority, so the engine's own unique pass keeps the most important request.
- `streamer_partial_loads` replaces the skip at `0x1f2137b` with a stub that climbs from the current
  level while the levels still fit the gate, then enters the engine's load path with that level and
  those tiles. The stub was checked by disassembling its bytes out of the source before any launch.

## In game

What the owner saw in the second race. On the grid the player's car was sharp in 1 to 2 s and the
road 5 to 10 s after, everything was loaded at the start, the grass ground looked perfect at every
pause, one grass section went blurry after a spin, an AI BMW M4 GT3 and its livery stayed blurry all
race, and some building sides were blurry near the last corner. In the first race the livery was
blurry for three quarters of a lap and the grass ground for good.

| | first race | second race, both fixes |
|---|---|---|
| kicks | 1,181 | 1,276 |
| drops, of them not admitted at all | 11,091, 6,781 | 2,600, 1,389 |
| drops the reload fix refused | 1,572 | 739 |
| loads turned away for space, per kick | 90 | 111 |
| loads cut to what fits | | 923, 429 MB |
| tile traffic, laps 1 to 4 | 13.0 MB/s | 3.1 MB/s |
| frame rate, median and p99, laps 1 to 4 | 71.5 fps, 13.87 and 18.41 ms | 70.4 fps, 13.97 and 18.16 ms |

The two races differ in spins and traffic and the GPU runs at its thermal limit in both, so the
frame rate difference says nothing either way. In the second race no record kept the lowest of
differing requests, 5,860 kept the highest and 1,277 more carried a feedback priority.

## What is left

The player's car and driver hold 487 MB of the pool at the top rank at all times, and the AI BMWs
rank like props, so while the field passed on lap 5 not one BMW level was admitted, 2,100 to 3,950
tiles of them turned away each kick. Ranked again offline over those 97 kicks:

| policy | player's car and driver | track | AI BMW |
|---|---|---|---|
| as it runs now | 487 MB | 387 MB | 0 |
| the car's 4096 levels ranked at 24000 | minus 127 MB | 512 MB | 0 |
| the car's 4096 levels ranked at 13000 | minus 152 MB | 541 MB | 0 |
| AI cars on the vehicle table | unchanged | 165 MB | 204 MB |
| the first of these and the vehicle table | minus 128 MB | 305 MB | 204 MB |

Lowering the player's car frees room the track takes, not the AI cars, and ranking AI cars as cars
takes it from the track. A 1 GB pool cannot hold this car, a field of several car models and this
track at full detail, and every change here decides what loses. The owner left the sharing as the
engine has it,
[DEC-018](../../decisions/DEC-018-a-full-texture-pool-keeps-the-players-car-first.md).

## Smaller findings

- The player's car and driver ask for every level of every texture on every kick of both races,
  the driver's suit at 4096 and shoes at 2048 among them, with no camera in the ranking.
- The `[streamer]` line's pool readout on the timeline thread read the engine's allocator after the
  game freed it at exit. The mod caught the fault, the game's crash handler logged it first,
  [BUG-022](../../bugs/BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash.md).
- The game stopping taking input in the second race was its pause menu with the HUD hidden. The
  log shows the pause at 21:03:18 and only camera changes after, and the streamer kept reporting.
