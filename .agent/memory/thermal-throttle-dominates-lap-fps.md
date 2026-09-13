---
name: thermal-throttle-dominates-lap-fps
kind: memory
description: the reference laptop's GPU throttles within two minutes of driving, the owner's undervolt held the clock flat for a day and hung the GPU, so it is off again and throttling is back
updated: 2026-09-13
links: [BUG-002-fps-drop-entering-new-track-sections, lap-2026-09-05-nordschleife, frame-time-mod-against-passive-2026-09-13, BUG-016-vram-overhead-grows-across-scene-loads]
type: project
---

On the reference machine (RTX 3060 Laptop, 6 GB, no MUX switch) the GPU reaches 87 °C about two
minutes into a lap, and the driver's software thermal slowdown holds the core near 1500 MHz against
a 1927 MHz peak for the rest of the lap, at 100 percent utilisation. Measured with the `nvidia-smi`
sampler on the Nordschleife lap of 2026-09-05. On 2026-09-13 the owner undervolted the card and set
the fans hard, and the clock sat at 1822 MHz with no throttle reason up to 84 °C for a day of
runs. The same afternoon the undervolted card hung mid lap (`DXGI_ERROR_DEVICE_HUNG`, the NVIDIA
driver's timeout in the Windows event log), and the owner took the undervolt off, so the throttling
is back.

It matters because a throttling card hides any change smaller than the throttle, which is why the
Reflex runs of 2026-09-12 showed nothing, and the frame time pairs of 2026-09-13 only resolved what
they did because the clock was flat.

Apply it by checking the sampler's clock and throttle reasons in every measured window before
reading fps, by cooling the card to the same temperature before each launch of a pair, and by not
comparing frame times taken before the undervolt came off with ones taken after. Memory figures
do not depend on the clock.
