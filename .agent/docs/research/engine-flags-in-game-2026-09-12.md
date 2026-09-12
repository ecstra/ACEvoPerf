---
name: engine-flags-in-game-2026-09-12
kind: doc
description: three parked runs measuring the never used engine flags that bear on frame rate, global illumination costs 3.2 percent and 416 MB of video memory and neither half is reachable without switching it off, the probe count is a dead knob, log_pso_on_creation emits nothing in the release build
updated: 2026-09-12
links: [engine-flags, TODO-016-run-the-four-untouched-in-game-engine-flags, optimisation-deepdive-2026-09-12, BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache, BUG-016-vram-overhead-grows-across-scene-loads]
---

# The in-game engine flags, measured

The deep dive found 178 of the 216 engine flags had never been mentioned in this repo. Four bear
on in-game behaviour rather than loading, which is the only thing the owner wants spent on now.
Three runs on 2026-09-12 settled two of them. No build was needed: any bool, int32 or double name
from the flag table works in the ini.

## The protocol, which was the point

The deep dive's measurement hazard said a lap cannot resolve any of this, and it was right. Each
run parks at the same spot on the Nürburgring, same car, same time of day and weather, cockpit
view, hands off, about two minutes. Runs are compared **aligned by seconds since the load ended**,
not by wall clock, and only over the overlap.

That alignment is not optional, because the machine drifts while standing still:

| seconds parked | fps | mean frame |
|---|---|---|
| 5 | 91.0 | 11.02 ms |
| 55 | 86.0 | 11.55 ms |
| 105 | 83.9 | 11.93 ms |

**8.3 percent lost in 105 seconds of doing nothing.** That is larger than every lever on the
current list. Any A/B that does not hold the thermal state constant is measuring the cooler, not
the change.

All three parked windows began at t=67 s by the same detector, which is the protocol working.

## Run A against run B: `no_gi`

119 parked seconds compared.

| | mean frame | fps | video memory |
|---|---|---|---|
| baseline | 11.507 ms | 86.9 | 4548 MB |
| `no_gi=true` | 11.139 ms | 89.8 | **4132 MB** |

**Global illumination costs 3.2 percent of frame time and 416 MB of video memory.**

The frame time half is a real signal and not noise: the median per second gain is 3.5 percent and
`no_gi` was slower in only 3 of the 119 seconds.

The memory half was not expected and is the larger number. On a card whose entire texture tile
pool is 1024 MB, global illumination holds more video memory than the whole overhead growth
BUG-016 has been chasing, which runs 24 MB to 360 MB. It is the largest single named consumer
this project has found.

## Run A against run C: `gibake_probes_per_frame`

112 parked seconds compared. The flag went from its default of 16 to **2**, deliberately
aggressive to size the ceiling rather than to look good.

| | mean frame | fps | video memory |
|---|---|---|---|
| baseline | 11.465 ms | 87.2 | 4548 MB |
| `gibake_probes_per_frame=2` | 11.509 ms | 86.9 | 4549 MB |

**Nothing.** 0.4 percent the wrong way, which is zero, and 63 of the 112 seconds were slower. The
video memory does not move by a megabyte.

So the cost `no_gi` removes is the shading lookup and the probe storage, not the per frame baking.
Cutting the bake rate by eight recovers none of it.

## What that means, and it closes the angle

There are exactly two global illumination flags in the whole engine, `no_gi` and
`gibake_probes_per_frame`. One is the entire feature and cannot ship, because turning it off makes
the lighting wrong and the mod corrects engine misbehaviour rather than trading picture for speed.
The other does nothing. There is no third knob and nothing in between.

**The global illumination angle is closed.** Three launches, a firm number on both halves, and a
negative that nobody needs to reopen. The 416 MB is worth remembering whenever video memory
pressure comes up again, as legitimate use rather than a defect.

## `log_pso_on_creation` emits nothing

Written correctly, at both the early and the late pass:

```
flag log_pso_on_creation = true (bool, was false) @00007FF73A2089C0 [Renderer.cpp, early]
```

And the game log contains **zero** pipeline creation lines, in any of the three runs. Since
`enable_pso_cache=false` forces every pipeline to be compiled fresh, pipelines certainly were
created, so the release build does not emit that line at all. The flag exists, it is settable, and
it is a dead diagnostic. Same family as the release build ignoring the flag table on the command
line.

This means the question it was meant to answer, whether pipeline state creation happens while
driving, still has no instrument.

## An unasked for finding: the PSO cache warnings do not stop with the flag off

Every one of the three runs has `enable_pso_cache = false` written at both passes, and every one
of them still logs the warning at each scene load:

| run | warnings | counts |
|---|---|---|
| A | 4 | 5, 30, 5, 52 |
| B | 3 | 17, 30, 1 |
| C | 3 | 17, 28, 2 |

`PSO Cache: N pipeline requests never completed, re-enabling them`, three to four times per short
session, in all three. BUG-015 was closed on the reading that switching the flag off takes that
path out of play, citing one such line in a 26 minute session. It does not take it out of play.
The rate tracks scene loads rather than the flag. See BUG-015 for what that does and does not
say about the headlight defect.

## Still open in this batch

`ai_run_dynamic_track` and `car_update_complete_max_interval` both act on cars other than the one
being driven, so they are inert in a solo hotlap by construction. They wait for a session with a
full grid.

## Two smaller things the logs volunteered

- **Tile streaming never stops.** Parked, engine off, nothing moving, the tile queue pulls about
  18 MB/s in a steady pattern of 18 requests every second second. Over the 119 second window that
  is 2.1 GB. Worth a look if texture streaming is ever revisited, because the scene is static.
- **Video memory is stable while parked**, 4548 MB against a 5226 MB budget, not moving by a
  megabyte across two minutes. Whatever drives BUG-016's growth happens at scene loads, not
  during play, which narrows it.
