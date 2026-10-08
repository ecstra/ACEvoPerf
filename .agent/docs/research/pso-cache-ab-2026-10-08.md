---
name: pso-cache-ab-2026-10-08
kind: doc
description: three launches on the reference machine measuring what the game's pipeline cache buys, about 4 to 5 s off a track or menu load once the cache file exists and one fewer stutter cluster leaving the pits, nothing while driving, and about 5 s extra on the run that builds the file
updated: 2026-10-08
links: [BUG-039-the-pso-cache-draws-some-materials-wrong, DEC-024-the-pso-cache-stays-off, engine-flags, telemetry]
---

# The PSO cache, on against off, 2026-10-08

The owner asked whether `enable_pso_cache` helps at all before it leaves the shipped ini (BUG-039,
DEC-024). Three launches on the reference machine, the RTX 3060 Laptop, the build of `0.4`, the same
graphics settings, a Ferrari 296 GT3 practice at the Nurburgring GP each time, menu, track, back to
the menu, quit. The logs are in `logs/pso-ab-20261008`.

| launch | cache | what was done |
|---|---|---|
| A | off, no cache file on disk | one lap |
| B | on, no file yet, so it builds one | load the session and quit, 26 MB written at quit |
| C | on, reading B's file | one lap, driven as A was |

Timeline and frame CSVs were on for all three. C ran with the game's `-log_debug=rendering` launch
option, which lists what the cache does, A and B without it. A GPU sampler was started but failed at
09:41 with an unknown error before A, so there are no GPU clocks for any launch.

## Loads

The game's own load profiler, total seconds per load.

| load | A off | B on, building | C on, warm |
|---|---|---|---|
| menu at start | 9.94 | 8.50 | 4.19 |
| Nurburgring GP | 19.25 | 24.72 | 14.64 |
| back to the menu | 4.27 | 4.19 | 9.17 |

Track resource streaming, the step the cache shortens, went from 13.72 s off to 10.47 s warm at the
track, and from 6.02 s building to 1.25 s warm at the first menu. A was the first launch of the
morning, so Windows may not have held the game's files in memory yet, which is why B rather than A is
the fair comparison for the first menu. The track had been loaded earlier that morning in another
session.

So a warm cache takes about 4 to 5 s off a load. The run that builds it pays about 5 s at the track
instead. C's return to the menu was 5 s slower than A's and B's, all of it in committing meshes, once,
which nothing here explains.

In C the debug log shows 353 pipelines found on disk and 70 merges of pipelines it did not have, many
of them mid lap since B never drove, and no `pipeline requests never completed` warning at all, where A
and B each logged three, for about 75 requests.

## Driving

Over the window from the end of the track load to the return to the menu, 134 s in A and 131 s in C.

| | A off | C warm |
|---|---|---|
| average | 86.1 fps | 85.4 fps |
| 1% low | 42.2 fps | 44.7 fps |
| p99 | 15.07 ms | 15.00 ms |
| frames over 16.7 ms | 84 | 45 |
| frames over 100 ms | 4 | 2 |
| slowest second | 12 fps | 32 fps |

Both laps are clean from about ten seconds out of the pits to the end. The difference is one cluster in
A about nine seconds after the load, three frames of 67 to 110 ms as the scenery first comes into
view, which C does not have. C's own new pipelines mid lap caused no stutter over 33 ms. Each side has a
stutter as the session starts and as it leaves for the menu.

## Reading

The cache saves a few seconds per load and one short stutter after leaving the pits, from the second run
on. It does nothing for the frame rate or the 1 percent low while driving. One run each, on one machine,
with no GPU clocks, so the load seconds are good to about a second and the stutter difference is one
cluster.
