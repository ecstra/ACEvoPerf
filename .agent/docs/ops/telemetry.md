---
name: telemetry
kind: doc
description: the log and CSV files the mod writes, their columns, and the external GPU sampler
updated: 2026-09-06
links: [proxy-architecture, tools, lap-2026-09-05-nordschleife, one-percent-low-hunt-2026-09-05]
---

# Telemetry

All files are written next to the game executable and overwritten on every launch. Copy them to
a session folder `logs/<name>-<yyyymmdd>-<hhmm>/` in the repo before analysing (`logs/` is
gitignored). `tools/telemetry_report.py SESSION_DIR` summarises a folder that holds them, with
`--from HH:MM:SS --to HH:MM:SS` for one stretch of a session.

## acevo_perf.log

Human readable. The configuration read from the ini, every engine flag written with old and new
value, the `auto sizes` line with the render adapter's memory and the pool and staging sizes
picked from it, every DirectStorage factory, queue and file event, per queue statistics every
`stats_interval_s` seconds, individual frames slower than `hitch_ms` (at most five per second)
with the streaming activity since the previous hitch, the swap chain's creation parameters and
a `[display]` line naming the adapter that owns the window's monitor, a warning when it is not
the render adapter.

With `[log] throw_log=1` the exe's import of `_CxxThrowException` is hooked and every C++
exception the game's own code throws is counted by throw site (the return address as an RVA)
with its mangled type name and, for `std::exception` types, the message of the first throw.
Every ten seconds with at least one throw the log gets a `[throw]` line with the count and the
eight busiest sites. Off by default, the hook costs nothing when a frame throws nothing.

## acevo_perf_timeline.csv

One line per second (`TimelineThread` in `src/telemetry/timeline.cpp`):

- `clock`, `t_s`: wall clock `HH:MM:SS` for lining up with the game log, seconds since attach
- `frames`, `fps`, `avg_ms`, `max_ms`: presented frames in the second, rate, mean and worst frame
- `hitch20`, `hitch_cfg`: frames over 20 ms and over `hitch_ms`
- `tile_req`, `tile_mb`: requests with destination `TILES` (texture streaming) and volume
- `tile_batches`, `tile_maxbatch`: submits on the tile queue and the largest batch in one submit
- `f2m_req`, `f2m_mb`: package to CPU memory requests and volume
- `gpumem_req`, `gpumem_mb`: CPU memory to GPU uploads and volume
- `submits`: DirectStorage submits on all queues
- `vram_used_mb`, `vram_budget_mb`, `vram_reservable_mb`: `QueryVideoMemoryInfo` on the discrete
  adapter, local segment
- `cpu_proc_pct`, `cpu_sys_pct`: game process CPU over all logical cores, whole system busy time
- `ws_mb`, `commit_mb`: working set and private commit of the game process

## acevo_perf_frames.csv

One line per presented frame: `t_s`, `frame_ms` (time since the previous present) and the
DirectStorage requests enqueued since the previous present, `tile_req` (texture tiles),
`f2m_req` (package to memory) and `gpumem_req` (memory to GPU). Frames longer than two seconds
are dropped as pauses. About 1 MB per ten minutes at 90 fps.

The report's spread section reads it: median, p99 over median, frames over 1.3, 1.5 and 2 times
the median with their share of the window's time, and how many of the slowest 1 percent carry
tile requests or uploads against the share of all frames that do.

## GPU sampler

Run beside the game, the report script joins it on the clock second:

```powershell
nvidia-smi --query-gpu=timestamp,utilization.gpu,utilization.memory,memory.used,clocks.gr,clocks.mem,power.draw,temperature.gpu,pstate,clocks_event_reasons.active,clocks_event_reasons.sw_power_cap,clocks_event_reasons.hw_thermal_slowdown,clocks_event_reasons.sw_thermal_slowdown,clocks_event_reasons.hw_power_brake_slowdown --format=csv -l 1 > gpu.csv
```

## Game log

`Saved Games\ACE\Logs\log-YYMMDD-HHMMSS.txt`. Launching with `-log_debug=rendering` raises the
rendering logger to debug, and the flag `log_pso_on_creation=true` adds a line per pipeline state
object. On 0.9.0 the debug rendering logger only adds PSO cache lines. The log also names the
adapter each monitor hangs off (`[Monitor] flat N (adapter ...)`), which told the 1 percent low
hunt that both displays are outputs of the integrated GPU.

## Deeper instruments, removed

The 1 percent low hunt of 2026-09-05 added a render thread sampling profiler, GPU timestamps per
command list batch, wait and queue hooks, a core speed probe, an input polling probe and a device
event log. They were removed on 2026-09-06 with their columns and log lines, the code is in the
history before commit `removed: the latency hunt instrumentation` and what they measured is in
`one-percent-low-hunt-2026-09-05`.
