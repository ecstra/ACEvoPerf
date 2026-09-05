---
name: lap-2026-09-05-nordschleife
kind: doc
description: what one full Nordschleife lap with telemetry showed about frame rate, streaming, VRAM, CPU and GPU
updated: 2026-09-05
links: [telemetry, BUG-001-texture-low-mip-shown-before-streaming, BUG-002-fps-drop-entering-new-track-sections, TODO-005-lap-two-experiments, thermal-throttle-dominates-lap-fps]
---

# Lap of 2026-09-05, Nordschleife

Setup: mod 0.2.0 with `staging_buffer_mb=128`, `enable_pso_cache=true`, `no_intro=true` and
`log_pso_on_creation=true`, game launched with `-log_debug=rendering`. Machine: RTX 3060 Laptop
(6 GB, 5226 MB budget), Ryzen 9 5900HX, 1920 by 1080 windowed, DLSS Ultra Quality, clouds,
volumetrics, motion blur and grass at Ultra, texture quality High. Session 17:10:03 to 17:29:36,
driving window 17:12:30 to 17:26:50. Numbers from `tools/telemetry_report.py`.

## Frame rate

- Whole session: 84,629 frames, mean 13.7 ms, p50 13.3, p95 16.9, p99 18.7, p99.9 50.9 ms.
- Driving window: per second mean 76 fps, per minute averages 75 to 83, per minute minimums 60
  to 72. No frame over 23 ms while driving. Every frame over 33 ms (308 in total) sits in the
  loading phase 17:10 to 17:12 or after the lap from 17:26:50.
- The first driving minute runs at 1838 MHz GPU clock and 85 °C. From the third minute the clock
  is 1490 to 1560 MHz at 87 to 88 °C, a loss of about 19 percent, and stays there.

## GPU

Utilisation 95 to 100 percent throughout the lap. Software thermal slowdown active in 98 percent
of the driving seconds. Power 86 to 100 W of a 159 W peak, so the power cap is not the limiter.
Temperature maximum 88 °C. Every slow cluster in the report has GPU utilisation at 99 or 100
percent with the clock between 1466 and 1604 MHz.

## Streaming

13,314 tile requests, 8.3 GB, in 802 of 960 seconds. Median batch 5 per submit, largest batch in a
second 119. Peak seen anywhere in the session 761 MB in one second (post lap load). File to CPU
memory traffic during driving was only 563 MB, GPU uploads from CPU memory 3.3 GB. Nothing in the
I/O path is near a limit.

## VRAM, CPU

VRAM peaked at 4592 MB against a 5226 MB budget, never within 100 MB. Game process CPU averaged
22 percent of all cores (peak 56), system 32 percent.

## Shader compilation

With `enable_pso_cache` on, the debug rendering logger shows `pso: found on disk` for the loads
and only single digit PSO events during the lap. Not a factor while driving.

## Reading

1. Frame rate on this machine is set by GPU work per frame and by cooling. The drop from around
   90 to the 70s is the thermal clock loss in the first two minutes, heavier sections then land in
   the 60s. Streaming, CPU, VRAM and shader compilation are all clear.
2. The texture pop in delay is not I/O. Bursts of hundreds of MB arrive within a second when the
   engine asks, so the second of low mips is the engine's feedback and request loop.
3. Lap two experiments are queued in TODO-005, one variable at a time.

## Laps two to four, same day

- Lap two (tile queue realtime, `texture_tier0`, resource preload): `texture_tier0` pinned textures to
  their lowest tier, tile traffic fell to 1.3 GB in four minutes with whole minutes at 1 MB. The
  other two showed nothing. All three reverted.
- Session of 17:47 (defaults, `veh_crashdumps`): three race loads, two car changes and a track
  change with no crash, the crashes of BUG-004 and BUG-005 were the staging buffers.
- Lap three (car reflections Medium, `gibake_probes_per_frame=8`): first stint 90 fps mean,
  p99 14.7 ms, lag 2 autocorrelation +0.34 from +0.50. Shadows flickered with the GI flag, it
  was reverted. Fullscreen changed nothing. The game log revealed the pool sizing: race 633 MB,
  restart 526 MB (BUG-010).
- Lap four (fixed 1024 MB pool, mesh cap 1433 MB, `max_frame_latency=1`, texture quality Ultra):
  pool constant across the race load and a restart, VRAM 4556 to 4614 MB driving, 5222 MB for
  one second at the race load, 82 to 83 fps mean per stint, p99 16.4 and 17.7 ms, lag 1 +0.03
  and lag 2 +0.22. Owner: road sharp, restart clean, pacing improved.

Still open after four laps: the second of low mips on first sight (BUG-001), grass pop in
(BUG-006) and the GPU spread between sections (BUG-009, TODO-002).
