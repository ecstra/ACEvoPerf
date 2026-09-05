---
title: Telemetry files written by the mod
updated: 2026-09-05
---

# Telemetry

All files are written next to the game executable and overwritten on every launch. Copy them to
a session folder outside the repo before analysing.

## acevo_perf.log

Human readable log. Contains the configuration read from the ini, every engine flag written with
its old and new value, every DirectStorage factory, queue and file event, per queue streaming
statistics every `stats_interval_s` seconds, individual frames slower than `hitch_ms` (at most
five per second) with the streaming activity since the previous hitch, and DXGI swap chain events.

## acevo_perf_timeline.csv

One line per second. Columns:

- `clock`: wall clock `HH:MM:SS`, for lining up with the game log
- `t_s`: seconds since the mod attached
- `frames`, `fps`, `avg_ms`, `max_ms`: presented frames in the second, their rate, mean and worst frame time
- `hitch20`, `hitch_cfg`: frames over 20 ms and over `hitch_ms`
- `tile_req`, `tile_mb`: DirectStorage requests with destination `TILES` (texture streaming) and their volume
- `tile_batches`, `tile_maxbatch`: submits on the tile queue and the largest number of requests in one submit
- `f2m_req`, `f2m_mb`: file to CPU memory requests (meshes, scenes, materials) and volume
- `gpumem_req`, `gpumem_mb`: CPU memory to GPU uploads (buffers and texture regions) and volume
- `submits`: DirectStorage submits on all queues
- `vram_used_mb`, `vram_budget_mb`, `vram_reservable_mb`: `QueryVideoMemoryInfo` on the discrete adapter, local segment
- `cpu_proc_pct`: game process CPU time over all logical cores
- `cpu_sys_pct`: whole system CPU busy time
- `ws_mb`, `commit_mb`: working set and private commit of the game process

## acevo_perf_frames.csv

One line per presented frame: `t_s` (seconds since attach) and `frame_ms` (time since the previous
present). Frames longer than two seconds are dropped as pauses. About 1 MB per ten minutes at 90 fps.

## External GPU sampler

For laptops the GPU clocks and throttle reasons matter. Run alongside the game:

```powershell
nvidia-smi --query-gpu=timestamp,utilization.gpu,utilization.memory,memory.used,clocks.gr,clocks.mem,power.draw,temperature.gpu,pstate,clocks_event_reasons.active,clocks_event_reasons.sw_power_cap,clocks_event_reasons.hw_thermal_slowdown,clocks_event_reasons.sw_thermal_slowdown,clocks_event_reasons.hw_power_brake_slowdown --format=csv -l 1 > gpu.csv
```

## Game log

`Saved Games\ACE\Logs\log-YYMMDD-HHMMSS.txt`. Launching with `-log_debug=rendering` raises the
rendering logger to debug level, and the engine flag `log_pso_on_creation=true` (set through the
mod ini) adds a line per pipeline state object created. Both are diagnostics only.
