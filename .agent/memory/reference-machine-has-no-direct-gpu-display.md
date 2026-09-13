---
name: reference-machine-has-no-direct-gpu-display
kind: memory
description: the reference laptop has no MUX and no output wired to the NVIDIA GPU, so every frame crosses to the integrated GPU and that path cannot be tested away on this machine
updated: 2026-09-13
links: [BUG-009-one-percent-lows-far-below-average, TODO-010-resume-the-one-percent-low-hunt, thermal-throttle-dominates-lap-fps]
type: project
---

The reference machine, an RTX 3060 Laptop 6 GB with an AMD integrated GPU, has no MUX switch. Its
external monitor is connected HDMI to HDMI through a port on the integrated GPU, and no USB-C to HDMI
cable is available. Both monitors are outputs of the AMD adapter, as the game's `[Monitor]` lines and
the mod's `[display]` warning show in every session, so every frame the NVIDIA GPU renders is copied
to the AMD GPU before it is shown.

It matters because the cross adapter present path is a lead for the 1 percent lows (BUG-009, the
render thread spent 0.9 ms of a median frame and up to 4 ms of a slow one in it on 2026-09-05), and
it cannot be removed on this machine to test it. The owner had answered this four times by
2026-09-13: "THERE IS NO MUX on this laptop and I do not have any cable that directly connects the
GPU to the monitor (Type c to hdmi, i do not have. I have connected via hdmi to hdmi)."

Apply it by treating the present path as fixed hardware here. Never ask the owner for a MUX, a
discrete or hybrid off mode, a GPU wired port or another cable. A measurement of that path needs a
different machine or a report from a user who has one.
