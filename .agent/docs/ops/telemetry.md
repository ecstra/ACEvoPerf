---
name: telemetry
kind: doc
description: the log and CSV files the mod writes, their columns, and the external GPU sampler
updated: 2026-09-13
links: [proxy-architecture, tools, lap-2026-09-05-nordschleife, one-percent-low-hunt-2026-09-05, tile-pool-reshuffle-2026-09-12]
---

# Telemetry

All files are written next to the game executable and overwritten on every launch. The log is
on by default. Every diagnostic is in the ini's `[developer]` section and off by default, the two
CSVs (`timeline=1`, `frames=1`), the streaming trace, request logging, the throw log, the package
file trace, the load sampler and the memory census. Copy
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
- `ws_mb`, `commit_mb`: working set and private commit of the game process

## acevo_perf_frames.csv

One line per presented frame: `t_s`, `frame_ms` (time since the previous present) and the
DirectStorage requests enqueued since the previous present, `tile_req` (texture tiles),
`f2m_req` (package to memory) and `gpumem_req` (memory to GPU). Frames longer than two seconds
are dropped as pauses. About 1 MB per ten minutes at 90 fps.

The report's spread section reads it: median, p99 over median, frames over 1.3, 1.5 and 2 times
the median with their share of the window's time, and how many of the slowest 1 percent carry
tile requests or uploads against the share of all frames that do.

## acevo_perf_streaming.csv

Written only with `[developer] streaming_trace=1`, from hooks on the engine's texture streamer
(`src/engine/streamer.cpp`) and from the DirectStorage queue proxy. Rows are buffered and written
once a second by the timeline thread. The header is `t_s,kind,a,b,c,d,e,f,g,h,i,j,k,l,m`, `t_s`
is seconds since attach on the same clock as the frames CSV, and the kind says what the letters
hold. Levels count from 0, the coarsest, and a tile is 64 KB.

- `kick`, one pass of the streamer, written at its first event. a kick number, b pool capacity in
  tiles, c admission budget, d records built, e records admitted, f tiles admitted, g records
  rejected, h load gate space at that moment, i load gate byte, then for the previous kick j
  textures that wanted a finer level, k loads turned away for space, l drops, m drops refused
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

## acevo_perf_memory.csv

Written only with `[developer] memory_census=1` (`src/telemetry/memory_census.cpp`), for BUG-016.
The import slots of `VirtualAlloc` and `VirtualAlloc2` are patched in every module, again at each
census for modules loaded later, and every commit is remembered with the three return addresses
above the call.

A census runs when the commit charge has stayed within 150 MB for 15 s, once at start and then each
time it has also fallen at least 700 MB from its highest reading since the last census, which is the
menu after a track unloads. It checks each remembered range against the address space, so released
and decommitted memory drops out, walks the address space, reads every heap with `HeapSummary`, then
compacts every heap with `HeapCompact` and reads again. Each census writes a `[memory]` line in the
log and rows under the header `t_s,kind,when,a,b,c,d,e,f,g,h`, `when` being `at start` or
`after an unload`, sizes in MB.

- `settled`. a the process commit charge (`PrivateUsage`), b private committed memory found by
  walking the address space, c heaps committed, d heaps in use, e remembered commits still committed,
  f mapped views committed, g images committed, h regions walked
- `heap`, every heap of 32 MB or more. a heap handle, b committed, c in use, d reserved
- `site`, every call site still holding 4 MB or more, largest first. a MB, b ranges, c to e the
  three return addresses as `module+0xRVA`, an address outside any module when the code was
  generated at runtime
- `compacted`. a seconds the compaction took, b commit charge after, c heaps committed after, d heaps
  in use after, e MB of commit charge returned, f seconds the reading before it took

It ran every ten seconds at first, and `HeapSummary` walks a heap under its lock. The game's main heap
holds 4 to 7 GB, so every census froze the game, 200 ms in the menu and 1.4 s on track
(`logs/memcreep-20260913/R-census-mod-on`), hence the settled trigger.

`HeapSummary`'s committed figure is not the commit charge. In a harness where 800 MB of small blocks
were freed, the heap had already released the memory, the commit charge fell to 68 MB, and
`HeapSummary` still reported 72 MB committed until `HeapCompact` brought it to 3 MB with the charge
unchanged. So a compaction only returns memory when `e` of `compacted` says so, and the heap columns
are the heap's own accounting. The same harness confirmed the partition otherwise, three 64 MB commits
as three sites, 64 MB committed in four pieces into one reservation as one site of four ranges with a
recommit inside it not counted twice, a release and an 8 MB decommit dropping out exactly, and nothing
left behind by four threads churning allocations.

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
