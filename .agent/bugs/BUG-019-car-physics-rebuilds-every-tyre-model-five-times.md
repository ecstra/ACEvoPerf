---
name: BUG-019-car-physics-rebuilds-every-tyre-model-five-times
kind: bug
description: the Ferrari 296 GT3 builds each of its four tyre models five times during the car physics load, 3.52 s of tyre builds for four distinct results, on the serial chain that ends the session load two seconds after the streaming phase
updated: 2026-09-12
links: [TODO-013-faster-session-loads, BUG-012-pit-lane-return-freezes-over-a-second, optimisation-deepdive-2026-09-12]
status: open
severity: bug
area: streaming
reported: 2026-09-12
parent: optimisation-deepdive-2026-09-12
---

## Problem

Loading a session with the Ferrari 296 GT3 logs `LOADING TYRE COMPOUND` twenty times for a car
with two compounds on four wheels. Only four distinct tyre models exist in the car, and the
engine's own log proves it: `SOFTNESS INDEX` takes exactly four values across the twenty
builds. Sixteen of the twenty builds recompute a result the engine already has.

It matters because those builds sit on the serial chain that decides when the load ends.

## Evidence

From `logs/loadsampler-20260912-1055`, one Nürburgring load, 16.69 s total.

The twenty builds run 10:56:22.282 to 10:56:25.798, a span of **3.52 s**, inside a car physics
phase that the profiler puts at 8.07 s. The four distinct `SOFTNESS INDEX` values are
0.5285858 and 0.47662127 for the front axle, 0.53585196 and 0.48079133 for the rear, each
repeated across both wheels of its axle. The per wheel pattern is the pair (Slick, Wet), then
Slick three more times.

For scale, the Mazda RX-7 in the same session logs eight builds with four distinct values in
173 ms. So the GT3's problem is both more builds and a far more expensive model per build,
roughly 176 ms against 22 ms.

**The chain it sits on.** The whole Nürburgring load, from the profiler and the physics log:

| clock | event |
|---|---|
| 10:56:12.901 | loading started |
| 10:56:15.132 | track resources streaming starts, loading pool boosted 2 to 11 workers |
| 10:56:17.121 | dynamic track preset load starts |
| 10:56:19.04 | car physics starts |
| 10:56:22.282 | first tyre build |
| 10:56:25.798 | last tyre build starts |
| 10:56:27.108 | car physics ends, 8.07 s |
| 10:56:27.541 | track resources streaming ends, 12.41 s |
| 10:56:27.27 to 29.563 | car graphics, 2.29 s, of which commit meshes is 1.69 s |
| 10:56:29.595 | loading complete, 16.69 s |

Car graphics starts 0.16 s after car physics ends and 0.27 s **before** streaming ends, so it
waits on car physics and not on streaming. The preset parse, the car physics and the car
graphics are one serial chain, and that chain finishes **2.05 s after the streaming phase
does**. The load ends when the chain ends, not when streaming ends.

The same shape holds in every load in that session: scene total equals car start plus car
total, to the millisecond, in all seven.

## Size

Removing the sixteen redundant builds is worth about 2.8 s of the chain if the builds cost
alike. The load only shortens until the chain stops being the tail, so the reachable saving at
the Nürburgring is bounded at the **2.05 s** the chain currently runs past streaming, after
which the streaming phase becomes the limit. Two caveats: the log lines mark the start of each
build and not its end, so 2.8 s is an upper estimate, and committing the car's meshes earlier
would then contend with streaming workers that are still busy, so part of the saving may be
given back.

It is car dependent. The RX-7's whole tyre phase is 173 ms, so there is nothing to win on it.

## Fix

Absent. There is no lever in the current proxy: fixing this means detouring the tyre compound
loader and memoising it on its inputs, which needs the inline detour and trampoline machinery
TODO-013 declined to build, on an address that moves with every game build, inside the physics
load. Memoising anything in a tyre model also risks sharing mutable per wheel state between
wheels, which would be a handling change rather than a load time win.

Worth telling Kunos either way: four distinct models, twenty builds.

## Verification

Absent.

## History

Raised by the startup-and-repeat angle of the deep dive of 2026-09-12 and killed by its
reviewer on the grounds that it was "worth zero seconds to a user on this machine today". That
verdict was wrong. It rested on the car physics phase being hidden behind the streaming phase,
and the profiler timeline above shows the chain running 2.05 s past the end of streaming in
the very log the reviewer cited. Reopened on re-verification the same day.
