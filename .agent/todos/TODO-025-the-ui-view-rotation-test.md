---
name: TODO-025-the-ui-view-rotation-test
kind: todo
description: one launch of a developer build that takes the UI view schedule through three schedules in 10 second turns at the Red Bull Ring GP, the game's rotation, the main view every frame and every view every frame, to show whether the UI rotation sets BUG-009's width now that the integrated GPU is ruled out
updated: 2026-09-15
links: [BUG-009-one-percent-lows-far-below-average, one-percent-lows-2026-09-14, ui-lag-deepdive-2026-09-14, BUG-024-pit-menu-pages-update-the-ui-one-frame-in-three, responsive-ui, telemetry, TODO-026-one-lean-etw-trace-of-the-slow-frames]
status: done
by: agent
area: render
born: 2026-09-14
done: 2026-09-15
---

## What

A developer build with `[developer] hud_schedule_test=1`. Every 10 seconds, in a shuffled order inside
each set of three, the mod changes how the game picks its UI views while the HUD is up.

- **The game's rotation.** One view a frame in turn, the HUD and then each dashboard display.
- **The main view every frame.** The HUD every frame with the dashboard displays taking turns beside it,
  what the responsive UI already does on menu pages.
- **Every view every frame.** What the game itself does in the main menu and the pause menu.

The log writes `[hud test]` lines with the time each schedule starts and every page the main view loads.
The launch is only a measurement, the schedule changes too often to feel.

One launch, the ini with `frames=1`, `timeline=1` and `ui_probe=0`, the machine idle and no window
switching. Ferrari 296 GT3 at the Red Bull Ring GP, the car and track with the largest ripple on disk, 7
minutes of hot laps at the monitor's own 165 Hz. The two 60 Hz stints of
[TODO-028](TODO-028-the-refresh-hold-test.md) follow in the same launch.

Each frame takes the schedule of its 10 second turn, with the first second of every turn and anything
outside `hud.html` dropped. Per schedule, frame time over its 101 frame local median gives p99, next to the
mean frame time and the 3 frame fold.

## Why

The deep dive of 2026-09-14 read in the exe that the game advances one UI view per frame in turn and found
a matching 3 frame ripple in every two display car's driving on disk. It was never tested, because the
present path through the integrated GPU looked like the larger part. On 2026-09-15 the owner's 5070
desktop with no integrated GPU showed the same gap (BUG-009), so the UI rotation is the first lead left
standing and the owner's own theory. Turns inside one launch take the section, the clock and the
temperature out of the comparison, which two launches could not.

The first plan, two launches with no build stepping the dashboard displays setting and `no_dash`, is
replaced by this one.

## Done when

BUG-009 records, per schedule, p99 over local median, the mean frame time and whether the 3 frame ripple
is gone. A schedule that narrows p99 over local median by 0.02 or more against the game's rotation, with
the mean frame time up by no more than 2 percent, is a fix the owner drives next. Under 0.01 for both
rules out the UI rotation as the width on this machine, and
[TODO-026](TODO-026-one-lean-etw-trace-of-the-slow-frames.md) runs next. The mean frame time of the main
view every frame against every view every frame says which view is heavy.

Done on 2026-09-15 with build `ca09393`, session `logs/hud-refresh-20260915`. The main view every frame
narrowed p99 over the local median by 0.064 at 165 Hz (1.241 to 1.177) and by 0.056 and 0.072 in the two
60 Hz stints, with the mean frame time up 0.02 ms, and the 3 frame ripple went from 1.00 to 0.04 ms. Every
view every frame costs 0.1 ms more for the displays. The numbers are in BUG-009, and the fix build is next.
