---
name: review-2026-09-sweep-review-telemetry
kind: review
description: the telemetry angle of the full review of main, hooks left installed in every module when the census cannot open its file and a sampler that picks the wrong threads after its first refresh, ten findings and one the render angle's verifier added
updated: 2026-09-20
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
when it cannot open its output file, which costs every allocation in the game for the rest of the
session and produces nothing. And the load sampler's thread picking is wrong after its first refresh,
which means the per thread numbers the project has been reading were measuring lifetime CPU rather than
what was hot during the load.

Ten findings, one breaks, five bug, three debt, one nit. F-11 was added on 2026-09-20 by the verifier
of `sweep/review-render`, which met the same shape in the render layer's adapter pick, and F-12 the
same day by the hunter of `sweep/review-proxy-core`, whose own fix made it matter more. F-05's
validation half is already closed by that branch, since the key is read there.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | an instrument that cannot start leaves nothing behind | pending | |
| 2 | the load sampler measures what it claims to measure | pending | |
| 3 | the CSVs mean what their headers say | pending | |
| 4 | the leftovers | pending | |

## Findings

### F-01: when the census cannot open its CSV the VirtualAlloc hooks stay installed in every module for the whole session
- severity: breaks
- found-by: review
- batch: 1
- status: open
- fix:

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

### F-02: a thread that was not a target last round presents its whole lifetime CPU as its delta, so the busiest thread pick is wrong on every refresh after the first
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

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
- status: open
- fix:

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

## Checked and clean

The suspected heap lock inversion in memory_census.cpp is not reachable, because ntdll's heap commits
through `NtAllocateVirtualMemory` rather than the import patched `VirtualAlloc`, so nothing ever holds
the heap lock and then waits on ours. The `TraceRow`, `WriteRow` and `Log` buffer arithmetic is correct
at every boundary the reviewer could construct, including the `_TRUNCATE` return of -1.

The census freeze itself, several seconds on a large heap, is documented and deliberate and is not
re-filed here.
