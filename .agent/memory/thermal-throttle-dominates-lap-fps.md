---
name: thermal-throttle-dominates-lap-fps
kind: memory
description: on the reference laptop the GPU hits its thermal slowdown within two minutes of driving
updated: 2026-09-05
links: [BUG-002-fps-drop-entering-new-track-sections, lap-2026-09-05-nordschleife]
type: project
---

On the reference machine (RTX 3060 Laptop, 6 GB, no MUX switch) the GPU reaches 87 °C about two
minutes into a lap and the driver's software thermal slowdown holds the core near 1500 MHz
against a 1927 MHz peak, for the rest of the lap. Utilisation stays at 100 percent. Measured with
the `nvidia-smi` sampler on the Nordschleife lap of 2026-09-05.

It matters because frame rate on this machine is a cooling and GPU workload question. Streaming,
CPU and VRAM were all clear during the same lap, so tuning them will not move the frame rate.

Apply it by measuring any render change against GPU clock and temperature, not fps alone, and by
telling the owner that a fan profile is the biggest single lever.
