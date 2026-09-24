---
name: review-2026-09-fix-review-shutdown
kind: review
description: the teardown angle of the full review of main, the log lock a terminated thread can still own when DllMain logs, four findings, one breaks, found by three reviewers independently
updated: 2026-09-24
links: [spec-reviews, house-rules-agent, BUG-022-pool-readout-faults-at-exit-and-the-game-logs-a-crash, reviews-index]
branch: fix/review-shutdown
status: open
---

# Review of the teardown path

## Summary

One angle of the full review of main run on 2026-09-20 at high. This angle is cross cutting rather
than one file. It asks what is still running, still hooked and still holding a lock when the game
exits.

Three reviewers working on different scopes arrived at the same defect from three different files, so
it is recorded once here rather than three times. BUG-022 was one instance of this class, a streamer
log line reading the engine's freed allocator at exit, and it was fixed for that one line. The lock
half of the same hazard is still open.

This branch owns the teardown path in `src/core/log.cpp`, `src/telemetry/timeline.cpp`,
`src/telemetry/load_sampler.cpp` and `src/engine/streamer.cpp`. Findings in those files that are not
about teardown belong to their own surface branches and are not repeated here.

Four findings, one breaks, two bug, one debt.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | nothing blocks on a lock a dead thread may own | fixing | 2026-09-24 |
| 2 | the timeline thread has a stop | pending | |

## Findings

### F-01: the detach path logs through a critical section a terminated thread can still own, and the game never exits
- severity: breaks
- found-by: review
- batch: 1
- status: fixed
- fix: b7dca1e, 2026-09-24, `LogDetaching` runs first on detach, and from then on `Log` takes its lock only when it is free and drops the line otherwise, the way `TraceFinalFlush` takes its own. The failure below was lighter than written, H-02. Microsoft documents that an `EnterCriticalSection` that would block while a process exits ends the process instead, so the game did exit, without its last two log lines and the rest of the teardown, and the fix keeps both.

At `ExitProcess` the kernel terminates every thread except the one running DllMain, and it does not
release the critical sections those threads held. `Log` at `src/core/log.cpp:26` holds `g_logCs`
across a `WriteFile`.

Failure: the timeline thread is inside that window when it is terminated. The loader then calls
DllMain with DLL_PROCESS_DETACH, holding the loader lock, and three separate call sites log on that
path. `StreamerDetach` calls `Report`, which calls `Log` (`src/engine/streamer.cpp:974`).
`StopLoadSampler` polls a flag that can never be set, correctly skips its summary, and then calls
`Log` anyway at `src/telemetry/load_sampler.cpp:451`. `LogClose` itself calls `Log("detached")` at
`src/core/log.cpp:34` before closing the handle. Any one of them blocks on a section owned by a dead
thread, so AssettoCorsaEVO.exe never leaves the task list, with no crash and no last log line.

The timeline thread widens the window on its own, because it is created and never signalled, joined or
stopped, and it logs every second through `ThrowLogTick`, `StreamerTick`, `MemoryCensusTick` and
`UiProbeTick`.

The project already solved this once for a different lock. `TraceFinalFlush` guards the trace lock with
`TryAcquireSRWLockExclusive` for exactly this reason. The log lock did not get the same treatment.

Found independently by three reviewers, from streamer.cpp, from log.cpp and from load_sampler.cpp.

### F-02: Log reads the file handle with no lock while LogClose closes it
- severity: bug
- found-by: review
- batch: 1
- status: fixed
- fix: 513f633, 2026-09-24, `LogClose` closes the handle under the lock, and `Log` reads it under the lock before it writes. `LogClose` runs only on detach, and at process exit every other thread is already gone, so the race needed a detach with live threads, which no run has shown.

`src/core/log.cpp:14` reads `g_log` outside the critical section, and `LogClose` closes it at line 35.

Failure: a live thread reads the handle, `LogClose` closes it, Windows reuses that handle value for
another file, and the live thread's `WriteFile` writes a log line into somebody else's file.

### F-04: StopLoadSampler reasons about a thread that will not answer and then calls Log regardless
- severity: debt
- found-by: review
- batch: 1
- status: fixed
- fix: b7dca1e, 2026-09-24, with F-01's fix the line after the poll takes the lock only when it is free, like every other line on detach.

`src/telemetry/load_sampler.cpp:446` to 450 already handle the case where the sampler thread does not
answer within 200 ms, and correctly skip `LogSummary`. Line 451 then calls `Log` unconditionally. The
code has the right instinct one line too early. Part of F-01's fix, recorded separately because the
reasoning next to it shows the hazard was half seen.

The hunter then ran on batch 1. It found nothing wrong in the two fixes and raised the four below, all
older than the batch, one of them lightening F-01.

### H-01: after DllMain returns, the mod's static CRT frees its containers under the heap lock, and with the streaming trace on a killed append can leave a string freed twice
- severity: debt
- found-by: hunter
- batch: 1
- status: wontfix
- fix: 2026-09-24. With the default settings the most it does is end the process at a free after every log line is written, which is what exiting does anyway. The double free needs a thread killed within a few instructions of an append, with a developer switch on.

`src/dllmain.cpp:151`. vcruntime's detach runs `_cexit` after DllMain, which runs the mod's 24 static
destructors, `g_toc`'s 64 MB block among them, then frees the CRT's per thread data. A dead thread
holding the process heap lock makes the first of those frees end the process, as the heap lock is a
critical section too. With `streaming_trace=1`, a job thread killed inside `TraceRow`'s append after the
string freed its old buffer and before it stored the new one leaves `g_pending` pointing at freed
memory, which `TraceFinalFlush` rightly leaves alone and its static destructor then frees again, a heap
corruption stop or an access violation the game's crash logger reports. How ntdll guards its fiber local
storage at exit is not documented.

### H-02: F-01's "the game never exits" does not match Microsoft's documentation
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: 2026-09-24, F-01's fix line and the reviews index say what happens instead.

The `EnterCriticalSection` page says that while a process is exiting, a call that would block ends the
process instead. So before b7dca1e the first log line on detach ended the process there, losing the
`[streamer] at exit` and `detached` lines and the rest of the teardown, and the game still left the task
list. Every one of the 60 newest session logs ends in `detached` except `airace-hang`, which never
reached detach. No player saw anything, so no CHANGELOG line is owed.

### H-03: with the streaming trace on, TraceFinalFlush still frees on the heap inside DllMain
- severity: nit
- found-by: hunter
- batch: 1
- status: wontfix
- fix: 2026-09-24. A developer switch, and the documented outcome is the process ending at that free with the `detached` line lost.

`src/telemetry/streaming_trace.cpp:87` and `:49`. The rows it writes can be too big for the heap's small
block path, so freeing them takes the heap lock, which a thread killed inside a heap call holds.

### H-04: the load sampler's poll at exit could never succeed, so every exit with it on waited 200 ms
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: ef69729, 2026-09-24, the poll, the summary it waited for and its flag are gone.

`src/telemetry/load_sampler.cpp:446` to `:450`. Windows ends the sampler thread before DllMain's detach,
so its exit flag stayed 0 and DllMain slept 40 times 5 ms under the loader lock. Both load sampler logs
on disk end with `samples in total` and then `detached`, with no final summary.

### F-03: the timeline thread has no stop path, so its ticks run through the game's own teardown
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`src/telemetry/timeline.cpp:143` creates the thread and nothing anywhere signals it. DllMain's detach
does not stop it.

Failure: main returns and the CRT runs the exe's static destructors with every thread still alive. The
timeline thread is one second into its next tick, `MemoryCensusTick` calls `HeapSummary` on a heap the
game has just destroyed, or `PatchEverywhere` walks a module being unloaded. The handled access
violation still stalls that thread 120 to 210 ms and still makes the game's crash logger write a report
naming the mod, which is exactly what BUG-022 was. Only the per tick caching of what each report prints
keeps this from being worse.
