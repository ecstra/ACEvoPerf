---
name: thermal-throttle-dominates-lap-fps
kind: memory
description: on the reference laptop the GPU throttled within two minutes of driving until the owner undervolted it, since then the clock holds flat and runs compare on GPU work alone
updated: 2026-09-13
links: [BUG-002-fps-drop-entering-new-track-sections, lap-2026-09-05-nordschleife, frame-time-mod-against-passive-2026-09-13]
type: project
---

On the reference machine (RTX 3060 Laptop, 6 GB, no MUX switch) the GPU used to reach 87 °C about
two minutes into a lap, and the driver's software thermal slowdown held the core near 1500 MHz
against a 1927 MHz peak for the rest of the lap, at 100 percent utilisation. Measured with the
`nvidia-smi` sampler on the Nordschleife lap of 2026-09-05. On 2026-09-13 the owner undervolted
the card and set the fans hard. Since then the clock sits at 1822 MHz with no throttle reason up to
84 °C, through a 33 minute session of laps, multiplayer and a 29 AI race.

It matters because a throttling card hides any change smaller than the throttle, which is why the
Reflex runs of 2026-09-12 showed nothing. With the clock flat, a controlled pair measures GPU work
per frame, and sessions still drift by about 0.6 fps parked.

Apply it by checking the sampler's clock and throttle reasons in every measured window before
reading fps, and by cooling the card to the same temperature before each launch of a pair.
