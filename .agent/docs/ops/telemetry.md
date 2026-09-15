---
name: telemetry
kind: doc
description: the log and CSV files the mod writes, their columns, and the external GPU sampler
updated: 2026-09-15
links: [proxy-architecture, tools, lap-2026-09-05-nordschleife, one-percent-low-hunt-2026-09-05, tile-pool-reshuffle-2026-09-12, memory-creep-2026-09-14, texture-streamer-camera-cuts-2026-09-14, responsive-ui, responsive-ui-rounds-2026-09-15, BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash, BUG-029-the-hud-restyles-most-of-its-page-while-driving]
---

# Telemetry

All files are written next to the game executable and overwritten on every launch. The log is
on by default. Every diagnostic is in the ini's `[developer]` section and off by default, the two
CSVs (`timeline=1`, `frames=1`), the streaming trace, request logging, the throw log, the package
file trace and the load sampler. Copy
them to a session folder `logs/<name>-<yyyymmdd>-<hhmm>/` in the repo before analysing (`logs/` is
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

With `[developer] throw_log=1` the exe's import of `_CxxThrowException` is hooked and every C++
exception the game's own code throws is counted by throw site (the return address as an RVA)
with its mangled type name and, for `std::exception` types, the message of the first throw.
Every ten seconds with at least one throw the log gets a `[throw]` line with the count and the
eight busiest sites. Off by default, the hook costs nothing when a frame throws nothing.

## acevo_perf_timeline.csv

One line per second (`TimelineThread` in `src/telemetry/timeline.cpp`). The thread runs whenever
the mod loads, because its tick also refills the hitch log budget and drives the throw log, and
it samples and writes only while a CSV is on. Columns:

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
- `ws_mb`, `commit_mb`: working set and private commit of the game process. The game's private
  commit includes its local VRAM one to one, so take `vram_used_mb` out before reading a commit step
  as RAM. The game log's `Request session start` memory reading sits 0 to 306 MB above the settled
  menu plateau, so memory comparisons use the timeline's plateau
  ([memory-creep-2026-09-14](../research/memory-creep-2026-09-14.md))

## acevo_perf_frames.csv

One line per presented frame: `t_s`, `frame_ms` (time since the previous present) and the
DirectStorage requests enqueued since the previous present, `tile_req` (texture tiles),
`f2m_req` (package to memory) and `gpumem_req` (memory to GPU). With `ui_probe=1` two more columns
carry the UI since the previous present, `ui_end_frame_ms` (the game's UI frame end, which waits for the
frame's UI job) and `ui_advance_ms` (every Cohtml `View::Advance`), both 0 otherwise. Frames longer than two
seconds are dropped as pauses. About 1 MB per ten minutes at 90 fps.

The report's spread section reads it: median, p99 over median, frames over 1.3, 1.5 and 2 times
the median with their share of the window's time, and how many of the slowest 1 percent carry
tile requests or uploads against the share of all frames that do.

## acevo_perf_streaming.csv

Written only with `[developer] streaming_trace=1`, from hooks on the engine's texture streamer
(`src/engine/streamer.cpp`) and from the DirectStorage queue proxy. Rows are buffered and written
once a second by the timeline thread. The header is `t_s,kind,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o`,
`t_s` is seconds since attach on the same clock as the frames CSV, and the kind says what the
letters hold. Levels count from 0, the coarsest, and a tile is 64 KB.

- `kick`, one pass of the streamer, written at its first event. a kick number, b pool capacity in
  tiles, c admission budget, d records built, e records admitted, f tiles admitted, g records
  rejected, h load gate space at that moment, i the loads byte (1 when the Resource Manager had no
  unfinished job as the kick was scheduled, 0 means the kick starts no load at all), then for the
  previous kick j
  textures that wanted a finer level, k loads turned away for space, l drops, m drops refused, n
  loads cut to the levels that fit, o tiles those loads carried
- `tex`, a texture seen for the first time. a Texture pointer, b its `ID3D12Resource` (the `res`
  of `req` rows), c level count, d package path
- `want`, an admitted texture below its admitted level, so a load is attempted. a kick, b texture,
  c current level, d admitted level, e level count, f tiles to load, g feedback mip, h feedback
  count, i feedback age in frames (mip -1 means no reading younger than the engine's limit), j 1
  when this reloads a level dropped within six kicks and 2 when that reload is the flip and pins
  the texture, k load gate space
- `drop` and `drop0`, a drop to a lower admitted level, and a drop to level 0 of a texture not
  admitted at all. a kick, b texture, c current level, d level kept, e level count, f tiles
  dropped, g to i feedback as above, j 1 when the fix refused the drop, k the verdict, l load gate
  space. Verdicts: 0 not a pinned texture, 1 refuse (or would refuse with the fix off), 2 the drop
  left the flip, 3 stale feedback, 4 the reading says the view moved away, 5 screen coverage
  changed, 6 the engine's 1024 tile margin is not free, 7 not admitted at all
- `req`, a texture tile request. a resource, b subresource, c tiles, d package offset, e bytes
- `reread`, a read into memory that repeats an earlier read exactly. a file (the `file=` of the
  `OpenFile` log line), b offset, c bytes, d how many times it has now been read

The log gets a `[streamer]` line every `stats_interval_s` with the same counts and the engine's
own tile pool figures (used, capacity, pending), and the file to memory queue's `[stats]` line is
followed by the total of repeated reads.

## UI probe

`[developer] ui_probe=1`, `src/ui/ui_probe.cpp`, for the game's Cohtml UI
([responsive-ui](../systems/responsive-ui.md) is what it measured into being). Once a second in the log:

- `[ui] end frame`: the UI frame end's count, total and worst wait, every view's `Advance` count, total and
  worst by size, resource and layout work (`ExecuteWork` types 0 and 1) with the part run on the frame
  thread, the resource work the responsive UI moved off it, and the UI clock against real time
- `[ui] restyles`: Cohtml's restyle passes of changed nodes with the nodes they restyled, and whole
  document restyles, plus a `slow restyle` line for each pass over 15 ms naming its first changed nodes
- `[ui] invalidations`: per kind the calls and the nodes marked (0 a child list change, 2 an id, 3 a class, 5
  a state such as hover, 7 an attribute such as `data-mode`), and the elements that marked the most
- `[ui] big child list change`: the first child list change of the second that marked 200 nodes or more, its
  element, marks, how many that second, and the call stack that led to it

Every five seconds `[ui] layout samples` lists where Cohtml's layout work was, innermost function and on the
stack, from the stack of a thread suspended only while it is inside that work. The style matching fix's
stubs show as `styles+0x...`.

The probe also adds a script to the menu and HUD view whose once a second `[ACEvoPerf] ui changes <page>`
line in the game log counts class, style, attribute and DOM writes, mouse events and transitions, what the
responsive UI's page fixes did, and every frame over 45 ms against what changed in the two frames before
it, with the ten busiest writes by element. On `hud.html` it counts no writes, the HUD's own come to about
20,000 a second and its bindings change the page where the counters cannot see. There it logs
`[ACEvoPerf] ui hud top level` for every change of the children of the HUD's top level, what was added and
removed and the model values the top level's conditions read (BUG-029).

## HUD schedule test

`[developer] hud_schedule_test=1`, in `src/ui/menu_refresh_fix.cpp`, for BUG-009. While the HUD is up it
changes how the game picks its UI surfaces every 10 seconds, in shuffled sets of three of the game's
rotation, the main view every frame and every view every frame. The log carries
`[hud test] t=<seconds> schedule <name>` at each turn and `[hud test] t=<seconds> page <url>` at each page
load, on the same clock as the frames CSV's `t_s`, so each frame can be given its schedule. Details in
[responsive-ui](../systems/responsive-ui.md).

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

The game's crash logger writes `[crash] Exception Detected` with a symbolised stack for every access
violation in the process, including one the mod's own `__try` handles, and stalls the faulting thread 120
to 210 ms doing it. The module it names is `DSTORAGE.dll` for the mod, with export names that mean nothing.
Check a lap with new hook code for the line (BUG-022 is one, the UI probe's node reads were fifteen in a
lap before 2026-09-15).

## Deeper instruments, removed

The 1 percent low hunt of 2026-09-05 added a render thread sampling profiler, GPU timestamps per
command list batch, wait and queue hooks, a core speed probe, an input polling probe and a device
event log. They were removed on 2026-09-06 with their columns and log lines, the code is in the
history before commit `removed: the latency hunt instrumentation` and what they measured is in
`one-percent-low-hunt-2026-09-05`.
