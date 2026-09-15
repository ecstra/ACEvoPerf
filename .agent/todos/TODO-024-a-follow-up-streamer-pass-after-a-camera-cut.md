---
name: TODO-024-a-follow-up-streamer-pass-after-a-camera-cut
kind: todo
description: build and test a follow up texture streamer pass after a camera cut whose own pass could not load, fired the first frame the Resource Manager's job count reads zero, behind an every other cut arm so one Red Bull Ring pit menu run carries its own control
updated: 2026-09-14
links: [BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring, texture-streamer-camera-cuts-2026-09-14, DEC-017-streamer-reload-fix-refuses-the-drop, telemetry]
status: open
by: agent
area: streaming
born: 2026-09-14
done:
---

## What

The first correction from BUG-021's deep dive
([texture-streamer-camera-cuts-2026-09-14](../docs/research/texture-streamer-camera-cuts-2026-09-14.md)),
built and tested in one owner run.

Build, in `src/engine/streamer.cpp`, on a fix branch.

- Mark camera cuts. Rewrite the call at `0x1D85A9A` (`e8 f0 6d 2b fe`, into thunk `0x3C88F`) to a stub
  that sets a cut flag and jumps on, inside hash checked regions over `0x1D85A83`, the thunk and
  `0x1F2C380`. The S1 hook takes and clears the flag, so the mod's own follow ups never set it.
- Arm at the end of a camera cut kick when its loads byte was 0, or it turned loads away for space and
  dropped tiles, or it used all 128 attempts with wants left. At most three follow ups, disarmed 2 s after
  the cut or on a kick that loads with none of those limits.
- Fire. Rewrite the call at `0x1F25F0C` (`e8 b3 aa 0e fe`, BeginFrame's pool pending getter). While armed
  and `[[streamer+8]+0x168]` reads 0, return a pending figure that puts used plus pending above the limit,
  so the engine's own branch at `0x1F25F1B` runs the kick. Check first the second way into the decision at
  `0x1F25E55` to `0x1F25E62`.
- Developer arm switch. A kick dropping 3,000 or more car and character tiles is a cut away and flips the
  arm, so every other cut is treated and every track shot is seen in both arms over two showcase cycles.
- Trace rows under `streaming_trace`: a `cut` row per cut away (kick, count, arm, car tiles), a `follow`
  row for every camera cut kick in both arms (reason, armed or not, delay, the loads byte of each follow
  up), the per frame count of deadline setter calls split camera or UI, and for 1 s after a cut the caller
  RVA of every Resource Manager push, which names the jobs that hold the count.

The owner's run.

1. Close the game and copy the test build's zip payload into the game folder.
2. Set `[developer] streaming_trace=1` and the arm switch on.
3. Launch, Red Bull Ring, Time Attack, Practice, Ferrari 296 GT3.
4. Stay in the pit menu for 7.5 minutes after it appears without touching anything, keeping the game
   window active.
5. Watch the car's rear decals about half a second after each cut back to the car and say whether every
   other return looks sharp sooner.
6. Leave the pits, drive two laps, press the camera key three times on the second lap.
7. Quit to desktop and hand over the logs.

## Why

A camera cut forces a pass that loads for the new shot before it drops the old one, often cannot load at
all, and then waits a full second. The livery's finest level is asked for 1.2 to 2.3 s after a cut back and
track shots take three passes at the shipped 1024 MB pool. The follow up keeps every engine decision and
only removes the wait.

## Done when

The run passes or kills it and BUG-021 holds the result. Pass means every treated cut kick that armed gets
a loading follow up before 0.7 s, the livery's finest level request comes before 1.0 s on treated returns
whose cut kick loaded, against 1.2 to 2.3 s on control returns, treated track shots have under a tenth of
their tiles missing by 1.5 s, frames after treated cuts are no worse than after control cuts, and on the
laps `follow` rows appear only within 0.1 s of a camera mode change. The run is void unless the control
arm reproduces the replayed shipped baseline (car drop of about 6,753 tiles at cut aways, about 6,800 car
tiles wanted at returns).
