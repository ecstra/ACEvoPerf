---
name: reference-machine-has-no-direct-gpu-display
kind: memory
description: the reference machine presented through its integrated AMD GPU until 2026-10-16, when a USB-C to DisplayPort cable put the monitor on the NVIDIA GPU, so runs before that date went through a cross adapter copy and runs after it do not
updated: 2026-10-16
links: [BUG-009-one-percent-lows-far-below-average, TODO-010-resume-the-one-percent-low-hunt, thermal-throttle-dominates-lap-fps, one-percent-lows-2026-09-14, mod-cost-2026-10-16]
type: project
---

The reference machine, an RTX 3060 Laptop 6 GB beside an AMD integrated GPU, has no MUX switch. Up to
2026-10-16 its monitor hung off an HDMI port wired to the AMD GPU, so the game rendered on the NVIDIA
GPU and every frame was copied across to the AMD one before it was shown, as the game's `[Monitor]`
lines and the mod's `[display]` warning show in every session of that time.

On 2026-10-16 the owner connected the monitor with a USB-C to DisplayPort cable on a port the NVIDIA GPU
drives, so from then on the game renders and presents on the same GPU. The first run over it
(`logs/direct-gpu-20261016`) parked in the pit box at 103.5 fps with every frame on an overlay plane
(`Hardware Composed: Independent Flip`), against 90.1 fps over HDMI that morning, with the card holding
1822 MHz where the morning run sat near 1665 MHz, so part of the gain is clock. A fullscreen apply changed
nothing there. The run ended in a driver timeout (nvlddmkm event 153, `DXGI_ERROR_DEVICE_HUNG`, then the
game's own "Failed to present swap chain"), which the owner put down to their undervolt.

It matters because the cross adapter present path was a lead for the 1 percent lows (BUG-009, the
render thread spent 0.9 ms of a median frame and up to 4 ms of a slow one in it on 2026-09-05). A machine
with no integrated GPU showed the same gap on 2026-09-15, so that path is not what makes the lows
([one-percent-lows-2026-09-14](../docs/research/one-percent-lows-2026-09-14.md)).

Apply it by checking which side of 2026-10-16 a session falls on before comparing its frame rate or
present times with another, and by reading the `[display]` line of a session's log for the path it took.
