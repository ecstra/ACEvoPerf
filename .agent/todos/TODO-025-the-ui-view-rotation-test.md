---
name: TODO-025-the-ui-view-rotation-test
kind: todo
description: two launches with no build at the Red Bull Ring GP, stepping the dashboard displays setting and then turning the dashboards off, to prove the UI view rotation sets BUG-009's 3 frame ripple, name the heavy view and measure what a steady schedule would buy
updated: 2026-09-14
links: [BUG-009-one-percent-lows-far-below-average, one-percent-lows-2026-09-14, ui-lag-deepdive-2026-09-14, BUG-013-one-percent-lows-drop-after-window-or-input-switch, telemetry]
status: open
by: agent
area: render
born: 2026-09-14
done:
---

## What

The first run from BUG-009's deep dive
([one-percent-lows-2026-09-14](../docs/research/one-percent-lows-2026-09-14.md)). No build, the shipped
ini with `[developer] frames=1` and `timeline=1`, the GPU sampler from the telemetry doc running, the card
cooled to the same temperature before each launch, and no window switching during a stint (BUG-013).

Launch A.

1. Ferrari 296 GT3 at the Red Bull Ring GP, hot laps for about 2 minutes with the dashboard displays
   setting at MaxTwo, as shipped.
2. Video settings, dashboard displays MaxOne, restart the session, 2 minutes of hot laps.
3. The same with All, 2 minutes.
4. The same back at MaxTwo, 2 minutes. Quit.

The game log must show `Enabled display for carId` after each restart.

Launch B. The same car and track with `[flags] no_dash=true`, two stints of 2 minutes. The mod log must
show `flag no_dash = true` and the game log no `Found N display(s)` line for the player's car. Afterwards
the setting and the flag go back.

Read each stint's frames folded by 1 plus its allowed display count (4 for All) and the block p99 over
median with the slot means evened out.

## Why

The deep dive read in the exe that the game advances one UI view per frame in rotation over the HUD and
the car's allowed dashboard displays, and found a matching 3 frame ripple in every two display car's
driving on disk and a 2 frame one for the one display Mazda. Its weight in the width runs from almost
nothing at the Nürburgring to 0.03 to 0.06 of p99 over median at the Red Bull Ring GP. This run proves the
mechanism before any change to the UI schedule is built.

## Done when

BUG-009 records three answers. Whether the cycle length follows 3, 2 and 4 in launch A and is gone in
launch B (killed if it stays 3 everywhere). Which view is heavy, the HUD if launch B's mean frame time is
0.2 ms or more above launch A's MaxTwo stints, a dashboard if it is 0.1 ms or more below. And what a steady
schedule buys at the Red Bull Ring GP, launch B's block p99 over median against launch A's MaxTwo stints,
worth the owner's decision only if it is 0.02 or more narrower with the average down by no more than
1 percent.
