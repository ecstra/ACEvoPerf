---
name: texture-streamer-camera-cuts-2026-09-14
kind: doc
description: BUG-021's deep dive with no new run, a camera cut forces a texture streamer pass that loads for the new shot before it drops the old one, starts at most 128 loads and none while any other resource job is unfinished, then waits a full second, which is the car's blur and at 1024 MB three passes of scenery blur, with the shipped fixes changing none of it and a follow up pass as the correction
updated: 2026-09-14
links: [BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring, TODO-024-a-follow-up-streamer-pass-after-a-camera-cut, texture-streamer-flip-2026-09-13, texture-streamer-overload-2026-09-13, directstorage-streaming, telemetry, DEC-009-pool-and-staging-sizes-by-card, DEC-017-streamer-reload-fix-refuses-the-drop, TODO-019-tile-upload-dedupe-done-properly, BUG-009-one-percent-lows-far-below-average]
---

# Camera cuts and the texture streamer, the deep dive

BUG-021's round of 2026-09-14. Four angles from the exe, the streaming traces and a replay of the
streamer's decisions, each checked by a second agent, then a critic and three gap rounds. No new
session. RVAs are for the 0.9.1 exe. A is `logs/rbr-blur-20260913/A-fix-off` (1024 MB, every streamer
fix off), C is `C-pool-1536-fix-off` (1536 MB, every fix off), and S is
`logs/memcreep-20260913/S-census-settled` (the reload fix on, no trace). The agents' scripts lived in
the session's scratch space and are not kept.

## What a camera cut does

1. **The cut forces a pass on that frame.** The showcase camera sets a flag for the one update its shot
   index changes (`0x1BD59AC`, `setne` into `[cam+0x61]`), the camera manager copies it into the render
   camera (`0x8AB017`), and the render frame tests it (`0x1D85A8A`) and calls `0x1F2C380` through the
   thunk `0x3C88F`, which only sets the streamer's kick deadline to now. In A every one of the 24 kick
   gaps under 0.95 s is a cut kick.
2. **The pass loads for the new shot first.** The kick `0x1F2D110` computes the load gate once at its
   start, `cap - (used + pending + 1024 - resident - pending_tracked)`, admits records in priority order
   against it, starts loads for admitted textures, and only then walks the map and drops what nothing
   requested (`0x1F2EDB9`). A pass's own drops never help its loads, and freed tiles reach the pool two
   frames later.
3. **Then it waits a full second.** On join `BeginFrame` sets the next deadline to now plus 1000 ms
   (`0x1F25E86` to `0x1F25EA1`).
4. **A pass starts at most 128 loads, and none while other resource jobs are unfinished.** The gather
   reads `[[streamer+8]+0x168]` at `0x1F29375`, and the update skips every load when it was nonzero
   (`0x1F212CF`) or once attempts reach `[streamer+0xE4]`, 128. That value is the Resource Manager's
   count of queued, running and parked jobs, incremented at every push (`0x27A0FE9`) and decremented only
   when a job returns done (`0x279F44D`), shared by 117 of the 206 push sites. The streamer's own level
   loads hold it too, one job per texture level chain plus about 0.1 to 0.2 s. So a kick loads only when
   nothing in the whole Resource Manager was pending when it was scheduled. At the pit menu cuts that
   happened on 12 of A's 24 cut kicks and 6 of C's 15, from other jobs, with no streamer chain in
   progress.

Nothing keeps a texture alive across a cut. No field records a last requested kick, and the map reset is
set only by a `.texture` file change callback. A dormant keep policy exists (`[streamer+0xDA]`, set to 1
by the constructor), and clearing it would keep the whole previous scene alive through the streamer's
map, so it is not a lever.

While driving the cut flag is set only by a camera mode change. The driving cameras' getter is
`xor al, al; ret` (`0x1BA83F0`), so a camera jump mid lap forces nothing, and across 42.6 minutes of
recorded laps only pit exits and camera key presses forced a pass (9 of 12). The other forcer is the UI
at `0xDE7605`, which calls the same deadline setter whenever its handler has texture requests pending,
so after page and scene loads the streamer runs a pass every UI frame with loads off.

## The car

- **The whole car goes at every cut away.** At each cut to a track shot the car and character textures
  drop to level 0, 6,753 tiles at all five of A's cut aways, because nothing in a track shot requests
  them. At A's kick 191 that was 165 car textures (5,081 tiles), 13 of the driver (352), 24 of
  characters and others (1,320) and 36 track textures (322). All came back 17 kicks later at their old
  levels.
- **The way back is a queue.** At the return about 200 car textures tie on the focus table (60000 minus
  500 per level at any distance, no feedback for car materials), and the 128 attempt quota splits them.

| return | load gate | loads | livery place | finest level requested after the cut |
|---|---|---|---|---|
| C k144 | 6,344 tiles | on, 122 started | 192 of 221, over the quota | 1.22 s |
| C k178 | 9,800 | on, 125 started | 66, loaded | 0.27 s |
| C k229 | 11,211 | off | 34 | 1.23 s |
| A k174 | 87 | on, 12 started and 116 turned away | 192 | 1.37 s |
| A k208 | 1,450 | off | 66 then 157 over the quota | 2.31 s |
| A k259 | 3,177 | off | 34 | 1.37 s |
| A k382 | 74 | on, 13 started and 115 turned away | 192 | 1.32 s |

A level chain then takes 0.20 to 0.35 s from want to the finest level's request, one level at a time.

## The scenery

- **The showcase cycle.** It repeats every 208 kicks with 11 shots, 7 of the car and 4 of the track,
  asking 21,500 to 22,650 tiles in a car shot and 10,000 to 15,150 in a track shot, on top of about
  900 untracked tiles and the 1,024 tile margin.
- **At 1024 MB a car shot pushes the track out.** A car shot fills the pool with about 7,700 tiles of
  car, driver and crew, so the track textures are dropped during every car shot. A cut to the track
  wants 5,267 to 7,177 tiles back into 184 to 404 tiles of room and settles in three passes, about
  three seconds (once four). At 1536 MB it wants 878 to 1,700 tiles and settles in two.
- **Some admitted tiles never load.** The admission budget counts the level 0 a drop keeps as free. In
  the fix1 race the admission promised a mean of 2,643 tiles beyond the room. In A's two heavy track
  shots the same 1,165 to 1,387 admitted tiles are missing every kick at a gate of 0 to 159, pit crew,
  cameraman, garages, floor and belt barriers among them.
- **No waste to reclaim.** 5 repeated requests of a held level in 330 s, and a pool rebuilt from the
  trace stays within 8 tiles of the engine's counter over 339 kicks. Four car textures are cooked with
  one level, 640 tiles in every shot, Kunos content.
- **VRAM has the room.** A median 3,747 MB in A's pit menu, and 4,258 MB at 1536 MB with 850 MB left at
  its peak.

## What the shipped fixes change there

A replay of the streamer's decisions reproduces every recorded decision of A and C with the fixes off,
and with the reload fix on it matches S over a matched 202 s to within 1 percent of traffic (5,063 MB
replayed, 5,033 MB in S).

- **The car numbers do not move.** With the rank fix, partial loads and the reload fix, the car drops
  6,753 tiles at all five cut aways, wants the same tiles at every return, and loads the livery on the
  same kick at all 7 returns. The reload fix never holds a car texture in A, C or the races.
- **The reload fix removes about a quarter of the pit menu's traffic**, 10,743 to 7,791 MB over A's
  stay, and holds 250 to 750 track tiles in the kick before each cut away, so one of five cuts settles
  a pass later.
- **Partial loads raise loads turned away in the heavy track shots** from 32 to 43 a pass while the
  tiles missing stay flat, so turned away counts do not compare across builds.
- **The rank fix has little to act on.** Track shots reject 0 to 11 records a kick.

## Corrections the dive makes

- **BUG-021's "11,000 to 20,000 tiles free through those cuts".** At the drop the gate was 0 to 339
  tiles, the new shot's loads having just taken it. 11,000 to 20,000 is the room during track shots,
  seen at the cut back. The drop still has no request behind it.
- **"Keep what is not needed while nothing waits"**, BUG-021's first angle, replayed. 13 MB saved and
  no livery at 1024 MB, 693 MB and no livery at 1536 MB. Keeping while free space covers it takes space
  from admitted loads on 235 kicks in A and 529 in a race. An unconditional drop to the level under the
  finest holds tiles on 1,264 of 1,281 race kicks.
- **A pool sized to the scene** is open again. The race's growth after a load was timed from the scene
  load, and VRAM grows only about 120 MB after the `Loading complete` line.
- **"A kick is also forced when used plus pending exceed the pool"** is true in code (`0x1F25F1B`) and
  forced no recorded kick, the lowest first row gate in six traces is minus 288 tiles.

## The corrections to the engine

1. **A follow up pass after a cut**, filed as
   [TODO-024](../../todos/TODO-024-a-follow-up-streamer-pass-after-a-camera-cut.md). When the cut's pass
   ran with loads off, turned loads away for space, or spent its 128 attempts, run the next pass through
   the engine's own path on the first frame the Resource Manager count reads zero, instead of waiting
   out the second. At most three, disarmed 2 s after the cut.
   - Where. A stub on the call at `0x1D85A9A` marks camera cuts. BeginFrame's pool pending getter call
     at `0x1F25F0C` returns a pending figure that takes the engine's own over capacity kick branch at
     `0x1F25F1B` while armed. The existing hooks arm it.
   - Expected. The livery sharp about 0.5 to 1.0 s after a cut back instead of 1.2 to 2.3 s, track shots
     settled in about 1 to 1.5 s instead of 3, at the shipped 1024 MB pool, with nothing admitted or
     dropped differently. While driving it arms only at pit exits and camera key presses, about 0.23
     extra passes a minute.
   - Risk. The stub sits on BeginFrame's every frame path, and the count can turn nonzero between
     BeginFrame and the gather. Why the engine gates streaming on a count shared with every resource load
     is unknown, the follow up keeps that gate and only drops the wait.
2. **Keep across cuts**, for bigger pools. The room a pass leaves goes to what dropped textures already
   hold, planned at the first drop of a kick, level by level. At 1536 MB, DEC-009's pool for 7 to 11 GB
   cards, it saves 72 percent of the cut reloads and the livery returns sharp 5 of 6 times. At 1024 MB it
   saves 7 percent and never keeps the livery. It waits on how the tile allocator behaves when it runs
   dry, since a kept tile is one a load cannot have (boot 1 lost about 4,000 tiles of gate beside a
   253.7 ms frame at 74.9 s).
3. **An admission budget that charges the level 0 a drop keeps.** The design first written would have
   taken every tracked texture's level 0 out of the load gate too, since the gate and the admission share
   the capacity call at `0x1F2E320`. A corrected form charges only the admission walk's local budget
   between `0x1F2E35C` and `0x1F2E408`. It adds no tiles, so the visible gain is doubtful.

## Still open

- What the tile allocator does when free tiles run out, from the free at `0xCE86B` to the allocate on
  the pool object. It blocks keep across cuts and bears on the reload fix's margin guard, whose comment
  in `src/engine/streamer.cpp` ("stalls rather than evicts") has no RVA behind it.
- Whether the renderer resets its temporal anti aliasing or upscaler history on the same cut flag
  (`0x1DC12A0`, the compare at `0x1DC1310`), which would leave a short soft moment with textures fixed.
- Which Resource Manager jobs hold the count at the pit menu cut kicks. TODO-024's trace row answers it.
- The unmeasured cost of a pass before its first hook, which matters for extra passes and for the bursts
  after page loads (BUG-009).

## Links

- **BUG-009.** A natural pass adds about 0.3 to 0.7 ms to the worst frame of its 150 ms window (boot 1
  laps median 13.4 against 12.7 ms, 24 against 7 of 760 windows over 1.5 times the local median).
- **The UI lag.** The UI forces a streamer pass every UI frame while it waits for textures after page and
  scene loads, each with loads off.
- **TODO-019.** Camera cuts cause 9 to 13 MB/s of reloads in the pit menu showcase at either pool size,
  and two identical 29 AI races differ by 28 percent in traffic, so cross run traffic numbers are
  unreliable.
- **`-log_debug=rendering`** still logs `Camera Cut this frame` (`0x3371230`), a free timestamp per cut.
