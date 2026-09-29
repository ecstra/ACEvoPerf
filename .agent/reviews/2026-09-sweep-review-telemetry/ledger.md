---
name: review-2026-09-sweep-review-telemetry
kind: review
description: the telemetry angle of the full review of main, hooks left installed in every module when the census cannot open its file and a sampler that picks the wrong threads after its first refresh, ten findings and two added by other angles' sub agents
updated: 2026-09-29
links: [spec-reviews, house-rules-agent, telemetry, reviews-index]
branch: sweep/review-telemetry
status: open
---

# Review of the telemetry instruments

## Summary

One angle of the full review of main run on 2026-09-20 at high. This angle covers
`src/telemetry/memory_census.cpp`, `load_sampler.cpp`, `timeline.cpp` and `streaming_trace.cpp`. All of
them ship disabled. The teardown findings in load_sampler.cpp and timeline.cpp belong to
`fix/review-shutdown`.

Two of these matter beyond the diagnostic itself. The census leaves process wide import hooks installed
when it cannot open its output file, which costs every VirtualAlloc commit in the game for the rest of
the session and produces nothing. And the load sampler's thread picking is wrong after its first refresh,
which means the per thread numbers the project has been reading were measuring lifetime CPU rather than
what was hot during the load.

Ten findings, one breaks, five bug, three debt, one nit. F-11 was added on 2026-09-20 by the verifier
of `sweep/review-render`, which met the same shape in the render layer's adapter pick, and F-12 the
same day by the hunter of `sweep/review-proxy-core`, whose own fix made it matter more. F-05's
validation half is already closed by that branch, since the key is read there.

Batch 1 closed with F-01 fixed, the census making its file before any hook, and the hunter's same
shape in the trace, the timeline, the frames and the load sampler fixed with it, each now saying when
its file cannot be made and the two that cost something every row or frame stopping for the run. It
added four from its hunter and seven from its verifier, one handed to the tools angle and one left as a
harmless race on a developer switch, and two launches with the files held confirmed every line.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | an instrument that cannot start leaves nothing behind | closed, runtime confirmed | 2026-09-29 |
| 2 | the load sampler measures what it claims to measure | fixing | 2026-09-29 |
| 3 | the CSVs mean what their headers say | pending | |
| 4 | the leftovers | pending | |

## Findings

### F-01: when the census cannot open its CSV the VirtualAlloc hooks stay installed in every module for the whole session
- severity: breaks
- found-by: review
- batch: 1
- status: fixed
- fix: 90c7a80, 2026-09-29, the file is made before any hook goes in, and the one failure after the hooks, `VirtualAlloc` not found, closes it again. The remembered ranges growing through a session that never unloads a track is the census working while it is on, and stays as it is on the owner's ack.

`src/telemetry/memory_census.cpp:362`. `PatchEverywhere` at lines 353 and 354 has already redirected
every module's `VirtualAlloc` and `VirtualAlloc2` import slot before `CreateFileW` at 361 is tried. On
failure the code logs "no census" and returns at 364 without unpatching, and `g_installed` stays false
so `MemoryCensusTick` returns at 378 and `Census()` never runs.

Failure: the CSV is still open in Excel from the previous run, or the folder is not writable. `Census()`
is the only place `g_ranges` is pruned, so from then on every committing VirtualAlloc in the process
pays `RtlCaptureStackBackTrace` plus a process wide exclusive SRW lock plus a `std::map` insert that is
never removed, for the rest of the session, producing nothing at all. The same leak exists on the happy
path whenever a session never drops the 700 MB threshold, so no second census ever fires.

Found independently by two reviewers.

The hunter then ran on batch 1. It found the reorder right in the source and the object code, every way
out of the install leaving nothing hooked and nothing it opened, and the timing unchanged, since the
install runs under the loader lock before any other thread, and raised the same shape in the other
three instruments, H-01 to H-03, and one wording finding on the fix's own comment, H-04.

### H-01: a streaming trace that could not make its file said nothing, and kept collecting rows only to free them
- severity: bug
- found-by: hunter
- batch: 1
- status: fixed
- fix: fe0679e, 2026-09-29, the first failure logs a line and ends the trace for the run, so no rows are formatted or kept, and no file starts partway through the session.

`src/telemetry/streaming_trace.cpp:51`. Retried every second, the create failed while last run's CSV
was open elsewhere, the rows were freed, and once it was closed the next flush truncated it and began a
file mid session with nothing marking the gap. `g_droppedRows` counts only the 64 MB cap, so the exit
line was silent too. Older than the batch.

### H-02: the timeline, frames and load sampler CSVs failed to open without a word, so an older run's file on disk read as this run's
- severity: debt
- found-by: hunter
- batch: 1
- status: fixed
- fix: 70c3ade, 2026-09-29, each logs the file it could not create and why.

`src/telemetry/timeline.cpp:16` to `:22` and `src/telemetry/load_sampler.cpp:343` to `:349`. The log
said the timeline had started and the sampler told the reader where its CSV was, and the copy in
`logs/<session>/` was then the previous run's. Older than the batch.

### H-03: with no frames CSV the present hook kept sampling into a buffer nothing empties
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: 70c3ade, 2026-09-29, the frames switch is turned off for the run when its file cannot be made.

`src/telemetry/timeline.cpp:129`. The buffer grew to its 200000 cap in about 23 minutes at 144 fps,
through 30 reallocations on the presenting thread. Older than the batch.

### H-04: the new comment said the census hooks cost every commit, where heap growth never reaches them
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: 908d628, 2026-09-29, every VirtualAlloc commit.

90c7a80 caused it.

The verifier then ran on batch 1. It found F-01 and H-01 to H-04 closed in the source and the object
code, every caller that reads the trace switch directly still feeding a log line of its own, and the
new lines' error read straight after the failed create, and raised the seven below.

### V-01: the telemetry report still reads a timeline or frames CSV the mod could not write as this run's
- severity: debt
- found-by: verifier
- batch: 1
- status: deferred
- fix: handed over to `sweep/review-tools` as its F-20, 2026-09-29, since `tools/telemetry_report.py` is that angle's.

### V-02: the trace's failure line said the trace is off for the run, where only its rows stop
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 1747f6b, 2026-09-29, the line and the comment say the `[writes]` and repeated read lines carry on.

fe0679e wrote it.

### V-03: g_cfg.frames is written on the timeline thread and read on others with no synchronisation
- severity: nit
- found-by: verifier
- batch: 1
- status: wontfix
- fix: 2026-09-29. A developer switch, and harmless in this build. The present hook rereads the byte on every call, and in all 116 sessions on disk that log both, the first present comes at least 4.1 s after the timeline starts, so the flag is down before any sample. The one visible effect is the timeline's start line printing either value.

`src/telemetry/timeline.cpp:63`, read at `src/render/frame_stats.cpp:88`. 70c3ade wrote it.

### V-04: the census's failure line was the only one that gave no reason
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 7529431, 2026-09-29, it gives the error.

Older than the batch, ceb4988.

### V-05: the run's paragraph had the launch as the menu and a quit, and H-01 to H-03 shown by it
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-29, the menu, Oulton Park and about four seconds on track, and none of H-01 to H-03's switches on.

c307b87 wrote it.

### V-06: the hunter's paragraph counted H-04 among the same shape in other instruments
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-29.

### V-07: the summary still said the census hooks cost every allocation in the game
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-29, every VirtualAlloc commit.

79177bc wrote it, and it stood through 908d628 and c307b87.

### F-02: a thread that was not a target last round presents its whole lifetime CPU as its delta, so the busiest thread pick is wrong on every refresh after the first
- severity: bug
- found-by: review
- batch: 2
- status: fixed
- fix: fd38f95, 2026-09-29, every thread's CPU time is kept at each refresh, targets or not, so each delta is measured from the last look and a thread new since then counts its whole time. The first refresh still picks by lifetime, having nothing to measure from.

`src/telemetry/load_sampler.cpp:246`. The lookup only finds the previous value in the current 24
targets, so every other thread in the process gets a previous of 0 and a delta equal to its total CPU
since it was created. The game has far more than 24 threads.

Failure: at the first refresh the 24 highest lifetime totals win. Two seconds later those 24 present
honest small deltas, a worker at one full core contributing about 2e7 ticks, while every non target
presents tens of seconds of lifetime CPU and wins the sort at line 252. The whole target set is swapped
out. The sampler then alternates between two sets chosen by lifetime CPU and largely ignores which
threads are actually hot during the load, which is the one thing the instrument exists to find.

### F-03: the per second CSV writes counters that the fifteen second summary zeroes, so every column sawtooths with no marker in the file
- severity: bug
- found-by: review
- batch: 3
- status: open
- fix:

`src/telemetry/load_sampler.cpp:271`. `WriteCsvLine` runs once a second and prints `g_total` and
`g_bucketCounts[]` raw, while `LogSummary` runs every fifteen seconds and memsets both at lines 326 to
328.

Failure: a reader sees samples climb for fifteen rows and then drop back to about one. Any tool
differencing consecutive rows gets a large negative delta fifteen times a minute, and any tool reading
the columns as cumulative reads only the last window. The header says nothing about the reset, and the
telemetry doc has no section for this file at all.

### F-04: heap handles are used well after the snapshot, and each swallowed fault costs the game 120 to 210 ms plus a crash report naming the mod
- severity: bug
- found-by: review
- batch: 4
- status: open
- fix:

`src/telemetry/memory_census.cpp:199`. `ReadHeaps` snapshots handles at 195 and then calls
`HeapSummary` on each, `CompactHeaps` does the same at 176 and 177, and a census does three such passes
over every heap with one pass alone taking 1.4 to 4.2 seconds.

Failure: a component unloads during that window and destroys its heap, and the next `HeapSummary` or
`HeapCompact` faults on a destroyed handle. The `__try` at 158 and 167 makes that safe in the sense that
the census continues, but in this game a handled access violation is not free. The crash logger stalls
the faulting thread and writes a line naming DSTORAGE.dll, which is BUG-022's shape, and here it can
fire once per heap per pass. A census that faults on five heaps costs an extra second and puts five
bogus crash reports in the player's game log.

### F-05: sample_us is read with no lower clamp, and zero turns the sampler into a full speed spin that suspends game threads as fast as the CPU allows
- severity: bug
- found-by: review
- batch: 2
- status: fixed
- fix: 8c3ccb1 and the batch 2 hunter round, 2026-09-20, on `sweep/review-proxy-core`. `sample_us` is read into a range of 100 to 1000000 and anything outside it is corrected with a line in the log.

`src/telemetry/load_sampler.cpp:354`, reading a value `src/core/config.cpp:117` never validates. Filed
against `sweep/review-proxy-core` as its F-05 for the validation half. Recorded here because the
consequence lands in this file: `SuspendThread`, `GetThreadContext` and `ResumeThread` on a game thread
tens of thousands of times a second from a `THREAD_PRIORITY_HIGHEST` thread, and the only way out is
killing the process. The telemetry doc does not cover it, which is the agent directory angle's
F-10. The claim originally made here that the key is not in the shipped ini is wrong,
`dist/acevo_perf.ini:63` has carried `sample_us=1000` with a note all along.

### F-12: the sampler's wait spins rather than sleeps at every interval near its default
- severity: debt
- found-by: hunter
- batch: 2
- status: fixed
- fix: 5003665, 2026-09-29, a wait too short to sleep calls `SwitchToThread`, which hands the core to any thread ready on it whatever its priority, and the comment says so.

`src/telemetry/load_sampler.cpp:365`. The comment above it says "Sleep the wait away rather than
spinning it. A spin here would hold a whole core at the highest priority and take it from the very
workers being measured." The code only sleeps when more than 1500 microseconds are left, and the
shipped `sample_us=1000` never reaches that, so the entire wait is `YieldProcessor` on a
`THREAD_PRIORITY_HIGHEST` thread.

The lower bound of 100 that `sweep/review-proxy-core` put on the key makes this worse at the bottom
of the range, ten thousand suspend and resume pairs a second with the wait between them spun rather
than slept. The bound is right as a bound, but its whole range sits inside the branch the comment
says it avoids, so either the comment or the threshold is wrong.

Raised by the hunter on batch 2 of `sweep/review-proxy-core`, recorded here because the file belongs
to this angle.

### F-06: the first timeline row reports process lifetime totals as one second of activity
- severity: bug
- found-by: review
- batch: 3
- status: open
- fix:

`src/telemetry/timeline.cpp:69`. Line 70 primes `lastReq` and `lastBytes` from the live counters before
the loop, and line 69 leaves `lastSubmits` and `lastBatches` at 0. `StartTimeline` is called from
`DStorageGetFactory`, which the game calls after it has already issued DirectStorage work.

Failure: row one of the timeline CSV carries every submit and every tile batch since attach in the
submits and tile_batches columns, so any maximum or mean taken over those two columns is wrong. The
asymmetry with line 70 shows this is an oversight rather than a choice.

### F-07: GetProcessHeaps is called twice with sixteen slots of slack, and overflowing that reads every slot as a null heap handle
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`src/telemetry/memory_census.cpp:176`. Line 175 sizes the vector to the count plus 16 and line 176 fills
it. When the true count exceeds the buffer, `GetProcessHeaps` stores nothing and returns the real count.
The code clamps with `std::min` and then iterates a vector still value initialised to nullptr.

Failure: `ReadHeap(nullptr)` and `CompactHeap(nullptr)` fault on every slot, each fault another crash
logger stall by F-04, so a 60 heap process would stall about nine seconds and write sixty bogus crash
reports, and it silently reports zero heaps committed. Sixteen new heaps inside the microseconds between
the two calls is unlikely, and the failure mode is loud. A retry loop is the fix.

### F-08: the pending buffer's capacity is thrown away on every flush, so the append path re-doubles from zero each second under the exclusive lock
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`src/telemetry/streaming_trace.cpp:66`. `TraceFlush` declares a fresh empty string and swaps it with
`g_pending`, so `g_pending` comes back with no capacity and the old buffer is freed.

Failure: at a few MB a second of rows the producers rebuild the buffer from scratch every second, about
20 reallocations each copying the whole buffer so far, and every one of them happens inside
`AcquireSRWLockExclusive` at line 39 with every streamer and DirectStorage hook in the process blocked
behind it. If the writer falls behind, which a memory census freezing the timeline thread does, the
buffer walks toward its ceiling and the last doubling copies tens of megabytes under that lock.

### F-09: if dxgi.dll is not loaded when the timeline starts, every video memory column is zero for the whole session and nothing says why
- severity: debt
- found-by: review
- batch: 3
- status: open
- fix:

`src/telemetry/timeline.cpp:61`. `FindRenderAdapter` calls `GetModuleHandleW(L"dxgi.dll")` and returns
null without logging. `StartTimeline` is called from `DStorageGetFactory`, which can precede the game
creating its device and factory.

Failure: the adapter stays null for the life of the thread, the video memory struct stays zeroed at line
99, and vram_used_mb, vram_budget_mb and vram_reservable_mb are 0 on every row. There is no retry and no
log line, so the owner analyses a CSV of zeros with no way to tell a genuine reading from a missing
adapter. The log at line 48 only fires on success.

### F-10: the enabled check lives inside TraceRow, so callers still evaluate their arguments when the trace is off
- severity: nit
- found-by: review
- batch: 4
- status: open
- fix:

`src/telemetry/streaming_trace.cpp:25`. `TraceRow` is variadic and tests the flag as its first statement,
so at `streamer.cpp:521` every streamer kick still evaluates eight raw offset reads into the engine's
streamer and kick structs plus `GateSpace(frame)` before calling a function that discards them. The cost
is small, and those are raw offset reads into engine memory on a code path that is supposed to be
entirely off, which is the fault surface BUG-022 came from. The other call sites guard properly with
`TraceOn()` at streamer.cpp:550, 596 and 652, or at install time in texture_writes.cpp:216.

### F-11: the timeline's adapter pick skips an adapter reporting no dedicated memory, so the video memory columns stay empty on those machines
- severity: debt
- found-by: verifier
- batch: 3
- status: open
- fix:

`src/telemetry/timeline.cpp:38` keeps an adapter only on `d.DedicatedVideoMemory > bestMem` with
`bestMem` starting at zero, so an adapter reporting none can never be selected and `best` stays null.

Failure: the same empty `vram_used_mb`, `vram_budget_mb` and `vram_reservable_mb` columns as F-09,
reached by a different cause, on any machine whose only GPU reports zero dedicated memory and keeps
everything in shared. Plenty of integrated parts do. `QueryVideoMemoryInfo` would answer for such an
adapter, the pick never gets that far.

Raised by the verifier on batch 1 of `sweep/review-render`, which fixed the same shape in
`DiscreteAdapter` (V-01 of that ledger, commit 0f35015). Left here because the file belongs to this
angle. Fixing it alongside F-09 is natural, they share the symptom and the function.

## The runs

Batch 1, one launch on 2026-09-29, `logs/telemetry-b1-20260929`, 10:18 to 10:19, the menu, a load into
Oulton Park, about four seconds on track and a quit, with `memory_census=1` for this launch alone and
`acevo_perf_memory.csv` held open with no sharing by another process, on 70c3ade's build.
`[memory] could not create acevo_perf_memory.csv, no census` in the same millisecond as the attach line
and no `census on` line after it, so nothing was hooked, a clean `detached` and no `Exception Detected`.
The timeline, frames, trace and load sampler switches were all off, so none of H-01 to H-03's code ran.

Batch 1 again, one launch on 2026-09-29, `logs/telemetry-b1b-20260929`, 11:50 to 11:52, the menu and a
quit, with `timeline`, `frames`, `streaming_trace` and `load_sampler` on for this launch alone and their
four CSVs held open with no sharing, on 7529431's build. Each logged its file with error 32, the
timeline and frames at 11:50:17.088, the load sampler at 11:50:17.166 and the trace at 11:50:18.089. The
`[writes]` lines and the load sampler's summaries carried on, as the new lines say, and the timeline's
start line printed `frames=1` a millisecond before the thread turned the switch off, V-03's one visible
effect. A clean `detached` and no `Exception Detected`.

## Checked and clean

The suspected heap lock inversion in memory_census.cpp is not reachable, because ntdll's heap commits
through `NtAllocateVirtualMemory` rather than the import patched `VirtualAlloc`, so nothing ever holds
the heap lock and then waits on ours. The `TraceRow`, `WriteRow` and `Log` buffer arithmetic is correct
at every boundary the reviewer could construct, including the `_TRUNCATE` return of -1.

The census freeze itself, several seconds on a large heap, is documented and deliberate and is not
re-filed here.
