---
name: review-2026-09-fix-review-shutdown
kind: review
description: the teardown angle of the full review of main, the log lock a terminated thread can still own when DllMain logs, four findings, one breaks, found by three reviewers independently, and one handed over from the Cohtml build guard angle
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

Four findings, one breaks, two bug, one debt, and a fifth, debt, handed over from the Cohtml build
guard angle.

Batch 1 closed with its three findings fixed, F-01 lighter than written since Windows ends a process
whose exit would wait on an abandoned lock rather than hanging it. It added four from its hunter and
nine from its verifier, and one fix of its own, the load sampler no longer waiting 200 ms at every exit
for a thread Windows had already ended.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | nothing blocks on a lock a dead thread may own | closed, runtime confirmed | 2026-09-24 |
| 2 | the threads the mod starts have a stop | fixing | 2026-09-24 |

## Findings

### F-01: the detach path logs through a critical section a terminated thread can still own, and the game never exits
- severity: breaks
- found-by: review
- batch: 1
- status: fixed
- fix: b7dca1e, 2026-09-24, `LogDetaching` runs first on detach, and from then on `Log` takes its lock only when it is free and drops the line otherwise, the way `TraceFinalFlush` takes its own. The failure below was lighter than written, H-02. Microsoft documents that an `EnterCriticalSection` that would block while a process exits ends the process instead, so the game did exit, without its last two log lines and the rest of the teardown. The fix keeps the teardown, and those two lines are still lost then, since the lock they need stays held. It stays graded breaks as the review filed it, and the reviews index leads with what it did.

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
- fix: b7dca1e and ef69729, 2026-09-24, the line takes the lock only when it is free like every other line on detach, and ef69729 removed the poll and the summary it waited for, H-04.

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
holding the process heap lock makes the first of those frees that takes the lock end the process, and
`g_toc`'s block always takes it. That goes by the heap's lock being a critical section, which is how
Windows builds it rather than anything Microsoft documents. With `streaming_trace=1`, a job thread killed inside `TraceRow`'s append after the
string freed its old buffer and before it stored the new one leaves `g_pending` pointing at freed
memory, which `TraceFinalFlush` rightly leaves alone and its static destructor then frees again, a heap
corruption stop or an access violation the game's crash logger reports. How ntdll guards its fiber local
storage at exit is not documented.

### H-02: F-01's "the game never exits" is contradicted by the EnterCriticalSection page
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: 2026-09-24, F-01's fix line and the reviews index say what happens instead.

The `EnterCriticalSection` page says that while a process is exiting, a call that would block ends the
process instead. F-01 followed the `ExitProcess` page, whose remarks warn of a deadlock for locks in
general. So before b7dca1e the first log line on detach ended the process there, losing the
`[streamer] at exit` and `detached` lines and the rest of the teardown, and the game still left the task
list. Of the 60 newest logs directly under a session folder, only `airace-hang` lacks `detached`, and it
never reached detach. Counting the runs one folder deeper, `memcreep-20260913/Q-passive` lacks it too, a
run whose GPU device was removed before the end. No player saw anything, so no CHANGELOG line is owed.

### H-03: with the streaming trace on, TraceFinalFlush still frees on the heap inside DllMain
- severity: nit
- found-by: hunter
- batch: 1
- status: wontfix
- fix: 2026-09-24. A developer switch, and the outcome is the process ending at that free with the `detached` line lost, going by the heap's lock being a critical section, which Microsoft does not document.

`src/telemetry/streaming_trace.cpp:87` and `:49`. The rows it writes can be too big for the heap's small
block path, so freeing them takes the heap lock, which a thread killed inside a heap call holds.

### H-04: the load sampler's poll at exit could never succeed, so every exit with it on waited 200 ms
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: ef69729, 2026-09-24, the poll, the summary it waited for and its flag are gone.

`src/telemetry/load_sampler.cpp:446` to `:450`. Windows ends the sampler thread before DllMain's detach,
so its exit flag stayed 0 and DllMain slept 40 times 5 ms under the loader lock. All four logs on disk
with the sampler on end with `samples in total` and then `detached`, with no final summary.

The verifier then ran on batch 1. It found no code defect, the three code commits doing what the ledger
says, raised the nine below, and found the Cohtml build guard angle's F-07 deferred to this branch and
never copied in, now F-05. The loop stopped there, all nine being precision in the record or in a
comment.

### V-01: F-01's fix line said the fix keeps the last two log lines, which it drops when a dead thread holds the lock
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, it says the fix keeps the teardown and those two lines are still lost then.

`src/core/log.cpp:61`. f27b281 wrote it.

### V-02: F-01 stayed graded breaks and the index still led with a game that never exits
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, the grade stays as filed with the reason in F-01's fix line, and the index leads with the process ending inside DllMain.

### V-03: H-02's title set F-01 against Microsoft's documentation, when the ExitProcess page says what F-01 said
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, the title names the `EnterCriticalSection` page and the body the `ExitProcess` page F-01 followed.

### V-04: H-02's count held only for logs directly under a session folder
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24.

`memcreep-20260913/Q-passive`, one folder deeper, has no `detached` either, a run whose GPU device was
removed before the end.

### V-05: H-04 said both load sampler logs, where there are four
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24.

### V-06: F-04's fix line described a poll ef69729 had removed
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24.

### V-07: the sampler's new comment said the window since the last summary is in the CSV, which holds only its bucket counts
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 4d7873a, 2026-09-24.

### V-08: the sampler's stop flag can stop nothing, and the comment said it matters to a thread still running
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 4d7873a, 2026-09-24, the comment says no caller reaches a live sampler. The flag stays for batch 2, which decides whether anything stops the mod's threads before the exit.

`src/telemetry/load_sampler.cpp:60`. A live stop would also race `CloseHandle(g_csv)` with the thread's
`WriteFile`, the race F-02 closed for the log.

### V-09: H-01 and H-03 went further than the evidence about the heap lock
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24.

Only a free that takes the heap lock can end the process, which `g_toc`'s block always does, and the
heap's lock being a critical section is how Windows builds it rather than anything Microsoft documents.

### F-03: the timeline thread has no stop path, so its ticks run through the game's own teardown
- severity: bug
- found-by: review
- batch: 2
- status: wontfix
- fix: 2026-09-24, with the owner's ack of the batch. With the default settings every tick the thread runs reads only the mod's own counters, `StreamerTick` since BUG-022, and the others return at once. The ticks that read what the game owns are developer switches, the census's heap walk and import patching above all, and the heap walk's reads are fault guarded, so the worst left is a caught fault and the crash logger's report at exit in a developer run.

`src/telemetry/timeline.cpp:143` creates the thread and nothing anywhere signals it. DllMain's detach
does not stop it.

Failure: main returns and the CRT runs the exe's static destructors with every thread still alive. The
timeline thread is one second into its next tick, `MemoryCensusTick` calls `HeapSummary` on a heap the
game has just destroyed, or `PatchEverywhere` walks a module being unloaded. The handled access
violation still stalls that thread 120 to 210 ms and still makes the game's crash logger write a report
naming the mod, which is exactly what BUG-022 was. Only the per tick caching of what each report prints
keeps this from being worse.

### F-05: the moved work thread runs forever and nothing at DLL detach drains it
- severity: debt
- found-by: review
- batch: 2
- status: open
- fix:

Handed over from the Cohtml build guard angle, its F-07, which deferred it to this branch on 2026-09-20
since the detach path is this branch's. Batch 1's verifier found it had never been copied in.

`src/ui/responsive_ui.cpp`. `StopMovingWork` runs only from `Hook_StopWorkers` and
`Hook_Uninitialize`, so a game that exits without reaching either, on a crash path or a shutdown that
skips `Library::Uninitialize`, has `MovedWorkThread` ended wherever it is, possibly inside Cohtml's
stylesheet parse or image decode holding a heap or Cohtml lock. That ledger called the result an exit
hang, which F-01's H-02 has since shown ends the process instead, and after b7dca1e the log no longer
waits on a lock at all. What is left is the heap lock of H-01 and the game's own teardown running beside
a live thread, F-03's shape.

That ledger also concluded the stop never runs at exit, from no drain line in any session. The drain's
lines are written only when work was left over or a call never came back, so their absence says nothing,
and the game's own log of batch 1's run shows `Uninitializing COHTML library!` at 15:47:14, two seconds
before the mod's detach. e2887ec has `Hook_Uninitialize` say so every time, so the next quit shows
whether the game's uninitialise goes through the hooked slot. Since d2896ad the line says the work is
stopped only when no moved call is still out, which is when the thread is parked on its semaphore,
holding nothing, as Windows ends it.

The hunter then ran on batch 2. It found F-03's reason right claim by claim, every default tick reading
only the mod's own state, and the moved thread parked and holding nothing after a stop that did not give
up, and raised the three below.

### H-05: the new line said the moved work was stopped even when the stop had given up on a call
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: d2896ad, 2026-09-24, `StopMovingWork` says whether a call is still out, and the line says so instead of stopped.

`src/ui/responsive_ui.cpp:488`. After a 5000 ms give up, the log would have read that the call did not
finish, that one never came back, and then that the work was stopped, while the original `Uninitialize`
freed the library under the live call. e2887ec caused it.

### H-06: the Cohtml ledger still said the game never calls the stop hooks
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: 2026-09-24, that ledger's H-23, its batch table and its note on the stop never running each carry a correction.

Its H-23 stayed low on the premise that nothing in a normal session calls slots 2 and 3, drawn from a
drain line that is written only when work was left over or a call is still out. Every one of the 86
sessions under `logs/` that reached `detached`, counting the runs one folder deeper and the early ones
with a dated game log, shows the game's `Uninitializing COHTML library!` 0.8 to 5.9 s before it. The six
that kept both logs without it never reached detach.

### H-07: the reviews index line did not move with batch 2
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: 2026-09-24.

The verifier then ran on batch 2. It found no code defect, `StopMovingWork` reading whether a call is
still out under its lock and the object code doing the same, F-03's reason right tick by tick, and
raised the four below. The loop stopped there, all four being precision in the record and one line's
tense.

### V-10: H-06's recount held only for part of logs/
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24, 86 sessions that reached `detached`, 0.8 to 5.9 s.

It counted the top level folders that reached `detached` and left out the runs one folder deeper, the
slip V-04 caught in batch 1, and the early runs whose game log carries a date in its name. The longest
gap, 5.86 s, is `frametime-20260913/P3-meshes-366`. a2b182a wrote it.

### V-11: the Cohtml ledger's H-23 correction repeated the same numbers
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24.

### V-12: three places said the stop's lines appear only when work was left over
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 4e15164 for the comment and the doc, 2026-09-24 for H-06, each adds a call still out.

The doc also said every `Uninitialize` writes the line, which is what the next quit has to show, and now
says the hook on slot 3 writes it each time it runs.

### V-13: with work left over the log said the engine stopped and then that it is shutting down
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 4e15164, 2026-09-24, the older line says the engine is stopping.

Both are written before the original `Uninitialize` runs, the past tense one since d10cb26.

## The runs

Batch 1, one launch on 2026-09-24, `logs/shutdown-b1-20260924`, a track and then a quit from the menu.
The log ends with the `[streamer] at exit` line and `detached`, and the game's own log has no
`Exception Detected`. The case the fix is for, a thread ended while it holds the log's lock, cannot be
forced from a run.
