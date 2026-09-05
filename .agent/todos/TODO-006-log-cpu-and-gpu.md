---
name: TODO-006-log-cpu-and-gpu
kind: todo
description: record CPU and GPU load alongside the frame data during test laps
updated: 2026-09-05
links: [telemetry, TODO-001-analyse-nordschleife-lap-telemetry]
status: done
by: owner
area: tooling
born: 2026-09-05
done: 2026-09-05
---

## What

"Log the cpu and gpu as well in your test i guess?"

## Done when

The per second timeline carries process and system CPU load (`cpu_proc_pct`, `cpu_sys_pct`) and
a `nvidia-smi` sampler records GPU utilisation, clocks, power, temperature and throttle reasons
next to it. Done in commit 0140743 (timeline) and documented in the telemetry doc, used on the
lap of 2026-09-05.
