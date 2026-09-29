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
the session and produces nothing. And the load sampler's thread picking is wrong after its first refresh.
It held threads by their lifetime CPU rather than by what was hot during the load, so the numbers for each
thread are sound for the stretches it was held, while which threads and stretches the project has been
reading came from the wrong pick.

Ten findings, one breaks, five bug, three debt, one nit. F-11 was added on 2026-09-20 by the verifier
of `sweep/review-render`, which met the same shape in the render layer's adapter pick, and F-12 the
same day by the hunter of `sweep/review-proxy-core`, whose own fix made it matter more. F-05's
validation half is already closed by that branch, since the key is read there.

Batch 1 closed with F-01 fixed, the census making its file before any hook, and the hunter's same
shape in the trace, the timeline, the frames and the load sampler fixed with it, each now saying when
its file cannot be made and the two that cost something every row or frame stopping for the run. It
added four from its hunter and seven from its verifier, one handed to the tools angle and one left as a
harmless race on a developer switch, and two launches with the files held confirmed every line.

Batch 2 closed with F-02 fixed, the sampler measuring every thread from its last look, and F-12 fixed
on the second try, the wait now blocking on a timer where the first fix still spun. Its two hunters
added twelve, its two verifiers fourteen and its run one. Among them were an older fault that filed a
sleep or a lock by the sort order of ntdll's names, corrections to a todo and two research docs that
leaned on the old pick, and one handed to the agent directory angle. One launch confirmed it, the
sampler using under a second of CPU in each 15 s window where the old loop held a whole core, and the
threads it held being the ones running. The same launch showed the per thread table hiding a short
load's workers, V-22, which joins batch 3.

Batch 3 closed with every first row, first report and window meaning what its label says. The frames
CSV and the hitch line count from the frame, hitch or pause before them, the sampler's CSV holds each
second on the clock every other file uses, the timeline reads all its baselines together and finds its
adapter on any machine, and the throw log prints the length it really covered. F-06 proved lighter
than filed. Its two hunters added nine, one of them the hitch line still carrying a whole load, and its
two verifier rounds eight. One launch confirmed it where a run can show it, and showed the per thread
table printing two rows under one name, V-31, which joins batch 4.

Batch 4 closed with the census reading a heap list of any length and looking each heap up again before
touching it, the trace and the frames buffer keeping their capacity, and the streamer reading engine
memory for trace columns only when the trace is on, on the kick row and on every want and drop. The
sampler's table prints each thread's id. Its two hunters added six and its two verifier rounds four, the
first round's tests showing this Windows handles a short heap list and a destroyed heap more gently than
filed. One launch confirmed it where a run can show it, the streamer's refusals and pins unchanged with
the trace off.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | an instrument that cannot start leaves nothing behind | closed, runtime confirmed | 2026-09-29 |
| 2 | the load sampler measures what it claims to measure | closed, runtime confirmed | 2026-09-29 |
| 3 | the CSVs and the log's reports mean what they say | closed, runtime confirmed | 2026-09-29 |
| 4 | the leftovers | closed, runtime confirmed | 2026-09-29 |

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

The saved runs show it milder than that, V-19. About a dozen threads, among them the render workers,
GameThread and Physics, kept their slots through whole windows, and only the remaining slots alternated
among long lived threads, so busy workers came and went rather than the whole set swapping.

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
- fix: 6a34f99, 2026-09-29, the wait blocks on a high resolution timer for its whole length. 5003665 had first swapped the spin's `YieldProcessor` for `SwitchToThread`, which returns at once when nothing else is queued on the processor, so the loop still spun, H-07.

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

Two hunters then ran on batch 2, one on each fix. They found the new pick right on every path, every
handle it opens kept or closed, nothing allocated while a thread is held suspended, and the sampling
itself unchanged, and raised twelve. The wait fix had not closed F-12, H-07 and H-08. The sampler had
been leaving a sleep or a lock to the sort order since it was written, H-05. The pick fix let a reused
thread id inherit a baseline and left a field unread, H-12 and H-13. H-10 and H-11 corrected comments,
and H-09, H-14 and H-15 corrected a todo and two research docs that leaned on the old pick or misread its
runs. H-06 and H-16 found F-06's shape in the frames CSV, the hitch line and the throw log, and join
batch 3.

### H-05: ntdll's Nt and Zw names for one system call share an address, and the sort chose which one a sample found, so a sleep or a lock could land in the wrong bucket
- severity: bug
- found-by: hunter
- batch: 2
- status: fixed
- fix: 09c7daf, 2026-09-29, a Zw name is read as its Nt twin before it is bucketed and labelled, so both twins file the same way whatever the sort does. All 489 Zw exports in this machine's ntdll have an Nt twin at the same address.

`src/telemetry/load_sampler.cpp:137` stored every name, `:179` sorted by address alone, and the bucket
patterns at `:82` to `:87` knew the Nt names and only one Zw name, `ZwWaitForWorkViaWorkerFactory`,
which 09c7daf removed since its Nt twin now matches the wait patterns. `NtDelayExecution`,
`NtRemoveIoCompletion` and `NtYieldExecution` were wait under the Nt name and system under Zw,
`NtAlertThreadByThreadId` lock or system, and `NtWaitForAlertByThreadId` lock or wait. The saved runs
went both ways. `logs/loadsampler-20260912-1128` printed `ntdll!ZwDelayExecution`, so every sample of a
sleeping thread went to system and ranked that thread as busy in the per thread table, and
`logs/telemetry-b1b-20260929` printed `ntdll!NtDelayExecution` beside `ntdll!ZwRemoveIoCompletion`.
Older than the batch, 1024e7e and 50b195f.

### H-07: 5003665 left the sampler's wait spinning whenever nothing else is queued on its processor, which is nearly every wait, so F-12's failure stood
- severity: debt
- found-by: hunter
- batch: 2
- status: fixed
- fix: 6a34f99, 2026-09-29, the wait blocks on a high resolution timer for its whole length, and each summary gives the sampler's own CPU time in the window so a run shows the difference.

`src/telemetry/load_sampler.cpp:376` to `:382`. `SwitchToThread` hands the processor over only when a
thread is already queued on it and otherwise returns at once, and the loop went straight back to
`QueryPerformanceCounter`. The game keeps about 4 of 16 logical processors busy, so a thread that becomes
ready goes to an idle one, and the sampler still held a logical processor at priority 12 for the whole
session, now with a system call each turn where the old `pause` at least left the core's other half its
share. 5003665 caused it by leaving the spin in place.

### H-08: a wait with 1501 to 1999 microseconds left called Sleep(0), which neither sleeps nor gives way to a worker below the sampler's priority
- severity: debt
- found-by: hunter
- batch: 2
- status: fixed
- fix: 6a34f99, 2026-09-29, the Sleep went with the spin.

`src/telemetry/load_sampler.cpp:380`, where `(DWORD)(leftUs / 1000) - 1` is 0 across that band. Older,
1024e7e.

### H-09: TODO-013's per worker numbers came from the old pick, and two of its conclusions did not hold
- severity: debt
- found-by: hunter
- batch: 2
- status: fixed
- fix: e16dd93, 2026-09-29, the todo says which threads the old sampler held, takes its worker numbers from the job lock run whose load window held all eleven, and drops the workers sampled row. It also stops counting the spin as real work, a slip in the same sentence.

`.agent/todos/TODO-013-faster-session-loads.md:117`, `:156` to `:175` and `:244`. Workers 0 and 1 were
held through a window that opens seven seconds before the load starts, so their low shares mixed the
menu with the load, and in `logs/joblock-A-off-1446` they look like the other nine. So the 10 percent end
of "between 10 and 27 percent" and "16 percent on the ones that are mostly waiting their turn" did not
hold, and 11 against 10 workers sampled was the printed top sixteen rather than the sampling. Older,
08dfb56, de8b7c4 and 471ffa6.

### H-10: the new comment and the ledger's fix line said the spin was gone
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: 6a34f99 and this ledger's update, 2026-09-29, the comment says the wait blocks on a timer and why the two waits before it spun, and F-12's fix line names both commits.

5003665 and 88b18c2 wrote them.

### H-11: the sampler's header said it reads a few thousand times a second, where the default reads about one thousand
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: a836997, 2026-09-29.

`include/acevo/telemetry/load_sampler.h:10`. Raised by both hunters. Older, 1024e7e.

### H-12: a new thread given the id of a thread that exited inherited that thread's CPU time as its baseline
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: 5293dcc, 2026-09-29, the creation time is kept beside the CPU time, and a thread whose creation time changed is measured from zero.

`src/telemetry/load_sampler.cpp:249` to `:257`. The sampler now closes its handle to every thread that is
not a target, so an id can be reused inside the window, and the new thread read as idle for one refresh,
against the comment. fd38f95 caused it, since before it the open handles to the targets kept their ids
reserved.

### H-13: Target::tid was written and never read
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: de750d4, 2026-09-29.

`src/telemetry/load_sampler.cpp:37` and `:265`. fd38f95 removed its only reader.

### H-14: two research docs said the old sampler's numbers covered every thread
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: a642c6d, 2026-09-29, both say the samples covered the two dozen threads the sampler held, and the optimisation deep dive's 107 windows are no longer all loads.

`.agent/docs/research/ui-lag-deepdive-2026-09-14.md:42` and `:223`, and
`.agent/docs/research/optimisation-deepdive-2026-09-12.md:381` and `:384`. Older, 0a207a6 and ae5ba22.

### H-15: the optimisation deep dive called 0x006AB180 the hottest game address in every load and the clock pair second and third in every load window
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: 9285b2b, 2026-09-29, both rankings are the whole runs' totals, 0x006AB180 leads 84 of the 107 windows, and in all six track load windows the job queue spin leads.

`.agent/docs/research/optimisation-deepdive-2026-09-12.md:422` and `:462`. In
`logs/loadsampler-20260912-1055` the spin at 0x0279FAC0 leads each track load with 695, 342 and 734
samples against 238, 77 and 193, and the clock pair is fifth or lower. Seen in passing by the hunter.

Two verifiers then ran on batch 2, one on the code and one on the records. The code verifier found F-02,
F-12 and every code finding of the hunters closed. The same loop, rebuilt outside the game, used about
440 ms of CPU in a 15 s window where the old one used a whole core, and kept every `sample_us` from 100
to 1000000 on average. Apart from three ntdll stubs too small to be sampled, no address in the system
modules the sampler reads now files under two buckets. It raised one, V-08. The record verifier found
every number the corrections wrote true against the logs, and raised thirteen. Nine are wording in
those corrections or in the older text beside them, V-09 to V-17. V-18 finds the deep dive's census,
which goes to the agent directory angle. V-19 finds F-02's failure milder in the runs than written, and
V-20 and V-21 are older numbers in TODO-013. The loop stopped there, with only record precision and one
comment left, and V-19's comment went in after the run.

### V-08: the load sampler's file header said each sample reads RIP and RSP, where it only ever read RIP
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 5ba0a6b, 2026-09-29.

`src/telemetry/load_sampler.cpp:5`. The phrase came from the removed render thread sampler, which passed
RSP to a stack walk. Older, 1024e7e.

### V-09: H-15's fix line counted five track load windows where the runs hold six
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-29, six. The one left out, t+30 of `logs/loadsampler-20260912-1055`, holds the first 6.5 s of the Nürburgring load, and the spin leads it too.

d33c91d wrote it.

### V-10: TODO-013's new note said "those rows" come from two windows, under the pits table whose rows come from a third
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: cba0628, 2026-09-29, the streaming rows.

e16dd93 wrote it.

### V-11: TODO-013 said the job lock run held all eleven workers, where only the run with the fix off did
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 046d4f4, 2026-09-29.

`logs/joblock-B-on-1450` prints ten, with no Worker 9. e16dd93 wrote it.

### V-12: the deep dive's "everything the load sampler recorded" covered two of that day's four runs
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 96195c9, 2026-09-29, the 107 windows of both runs, as the doc says elsewhere. Over all four runs 0x006AB180 is still the hottest, 13,308 of 52,460.

9285b2b wrote it.

### V-13: the deep dive said the engine's clock takes roughly 3 percent of a load, which holds only for the two main Nürburgring windows
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 37d6790, 2026-09-29, about 3 percent of those two windows and 1.2 to 1.6 percent of the Red Bull Ring and online loads.

`.agent/docs/research/optimisation-deepdive-2026-09-12.md:469`. The clock pair with
`RtlQueryPerformanceCounter` is 2.94 and 2.90 percent of the two main Nürburgring windows, 1.57 at Red
Bull Ring and 1.16 on the online load. Older, ae5ba22, standing in the paragraph 9285b2b edited.

### V-14: the reviews index said every batch but the telemetry angle's second was pending, where its first is closed
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-29.

`.agent/reviews/INDEX.md:27`. 88b18c2 wrote it.

### V-15: three details in the hunters' entries did not match the code or the history
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-29. H-05's patterns held one Zw name, H-06's 31,702 is the three request columns together, and H-09 names 471ffa6 beside the other two commits.

d33c91d wrote them.

### V-16: the ledger was not ordered by batch as the spec asks
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-29, F-03, H-06 and H-16 sit with batch 3 and F-04 with batch 4.

d33c91d put H-06 and H-16 among batch 2's blocks, and F-03 and F-04 had sat between F-02 and F-05 since
79177bc.

### V-17: two lines in the deep dive were left unwrapped, and three sentences the batch touched kept a colon in the middle
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 366aa78, 2026-09-29.

`.agent/docs/research/optimisation-deepdive-2026-09-12.md:424`, `:427` and `:467`, and
`.agent/todos/TODO-013-faster-session-loads.md:183`. 9285b2b left the long lines. The colons are older
text in the sentences it and e16dd93 edited.

### V-18: the deep dive says the census five verdicts leaned on does not exist, where it is the load sampler's printed game code rows over that day's four runs
- severity: nit
- found-by: verifier
- batch: 2
- status: deferred
- fix: handed over to `sweep/review-agent-dir` as its F-25, 2026-09-29, since the deep dive is that angle's.

`.agent/docs/research/optimisation-deepdive-2026-09-12.md:311` to `:313`, `:324` and `:350`. The four
runs print 43,979, 3,556, 2,506 and 2,419 game code samples, which add up to the 52,460 the verdicts
cite. Older, ae5ba22.

### V-19: F-02's failure and the pick fix's comment said the whole target set flipped every refresh, where about a dozen threads kept their slots
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: e293c18, 2026-09-29, the comment says the busiest threads kept their slots and only the rest went back and forth, and the note under F-02 says the same.

`src/telemetry/load_sampler.cpp:257`. In `logs/loadsampler-20260912-1128` t+60 eleven threads hold a
full share of samples through the window and only the rest alternate. 79177bc and fd38f95 wrote it.

### V-20: TODO-013's spin table left out the Nürburgring load's first window and put every other window at 0.1 to 0.2 percent
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 0610684, 2026-09-29, t+30 joins the Nürburgring row at 2.2 percent, the five windows holding the menu scene's own loads read 0.2 to 0.9 percent, and the other 93 read 0.3 percent or less.

`.agent/todos/TODO-013-faster-session-loads.md:145`. There were 99 other windows, not 100. Older,
08dfb56.

### V-21: TODO-013 gave the spin's share of its load window as its share of every sample
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: d52bb55, 2026-09-29.

`.agent/todos/TODO-013-faster-session-loads.md:258`. 8.3 percent is 1,119 samples of the load window.
Over the run it is about 3 percent. Older, 471ffa6.

### F-03: the per second CSV writes counters that the fifteen second summary zeroes, so every column sawtooths with no marker in the file
- severity: bug
- found-by: review
- batch: 3
- status: fixed
- fix: cb7fccf, 2026-09-29, the CSV keeps counts of its own, zeroed at each line, so a row holds its own second, and the summary's fifteen second counts stay the summary's.

`src/telemetry/load_sampler.cpp:271`. `WriteCsvLine` runs once a second and prints `g_total` and
`g_bucketCounts[]` raw, while `LogSummary` runs every fifteen seconds and memsets both at lines 326 to
328.

Failure: a reader sees samples climb for fifteen rows and then drop back to about one. Any tool
differencing consecutive rows gets a large negative delta fifteen times a minute, and any tool reading
the columns as cumulative reads only the last window. The header says nothing about the reset, and the
telemetry doc has no section for this file at all.

### F-06: the first timeline row reports process lifetime totals as one second of activity
- severity: bug
- found-by: review
- batch: 3
- status: fixed
- fix: ef1556a, 2026-09-29, the submit and tile batch baselines are read with the request ones.

`src/telemetry/timeline.cpp:69`. Line 70 primes `lastReq` and `lastBytes` from the live counters before
the loop, and line 69 leaves `lastSubmits` and `lastBatches` at 0. `StartTimeline` is called from
`DStorageGetFactory`, which the game calls after it has already issued DirectStorage work.

Failure: row one of the timeline CSV carries every submit and every tile batch since attach in the
submits and tile_batches columns, so any maximum or mean taken over those two columns is wrong. The
asymmetry with line 70 shows this is an oversight rather than a choice.

The failure does not happen as filed. The timeline starts inside the game's first `DStorageGetFactory`,
before any DirectStorage work can exist, so every counter is zero there, and the saved CSVs show the
start up burst of about 108 submits beginning within the first three rows, its own tile requests
beside it. What stood was a baseline read for two columns and not the other two, a few milliseconds
apart.

### F-09: if dxgi.dll is not loaded when the timeline starts, every video memory column is zero for the whole session and nothing says why
- severity: debt
- found-by: review
- batch: 3
- status: fixed
- fix: 9e620c4, 2026-09-29, the timeline loads dxgi.dll from System32 itself, the way the auto size fallback does, and logs a line when it has no factory or no adapter. In 98 of the 99 saved runs with the timeline on it found its adapter, and the other had its CSVs held on purpose. dxgi.dll is a static import of the exe, so on 0.9.1 it is always loaded in time and the load only adds a reference.

`src/telemetry/timeline.cpp:61`. `FindRenderAdapter` calls `GetModuleHandleW(L"dxgi.dll")` and returns
null without logging. `StartTimeline` is called from `DStorageGetFactory`, which can precede the game
creating its device and factory.

Failure: the adapter stays null for the life of the thread, the video memory struct stays zeroed at line
99, and vram_used_mb, vram_budget_mb and vram_reservable_mb are 0 on every row. There is no retry and no
log line, so the owner analyses a CSV of zeros with no way to tell a genuine reading from a missing
adapter. The log at line 48 only fires on success.

### F-11: the timeline's adapter pick skips an adapter reporting no dedicated memory, so the video memory columns stay empty on those machines
- severity: debt
- found-by: verifier
- batch: 3
- status: fixed
- fix: 3c7b742, 2026-09-29, an adapter reporting no dedicated memory is taken when nothing better has been found, the rule the auto sizes already use.

`src/telemetry/timeline.cpp:38` keeps an adapter only on `d.DedicatedVideoMemory > bestMem` with
`bestMem` starting at zero, so an adapter reporting none can never be selected and `best` stays null.

Failure: the same empty `vram_used_mb`, `vram_budget_mb` and `vram_reservable_mb` columns as F-09,
reached by a different cause, on any machine whose only GPU reports zero dedicated memory and keeps
everything in shared. Plenty of integrated parts do. `QueryVideoMemoryInfo` would answer for such an
adapter, the pick never gets that far.

Raised by the verifier on batch 1 of `sweep/review-render`, which fixed the same shape in
`DiscreteAdapter` (V-01 of that ledger, commit 0f35015). Left here because the file belongs to this
angle. Fixing it alongside F-09 is natural, they share the symptom and the function.

Batch 2's hunters raised two more of F-06's shape, in files whose own angles have merged.

### H-06: the frames CSV and the hitch line count requests from a snapshot only a recorded frame or a logged hitch moves, so the first row and the first hitch carry everything since attach
- severity: bug
- found-by: hunter
- batch: 3
- status: fixed
- fix: 67733f5, 2026-09-29, every present of the timed chain moves the frame baselines, its first and one after a pause included, and the hitch baseline is set at the chain's first present and moved by every hitch, logged or not. The hitch line after a pause came with 23cdc04, H-17.

`src/render/frame_stats.cpp:94` to `:99`, with the returns at `:67` and `:70` skipping it, and the hitch
snapshot at `:79` to `:84`. Row one of a frames CSV carries about 9,000 file to memory requests on one
frame where the median is 0, and a frame recorded after more than 2 s without one carries that stretch,
31,702 requests across the three request columns on a 1344 ms frame in
`logs/lap10-sampler-20260905-2219`. The first `[hitch]` line of a session reports about 8,900. F-06's
shape in the render layer's file, taken here since that angle has merged. Older, de460b9, 57b43fb and
5ef8d7d.

### H-16: the throw log's first report counts everything since attach and calls it ten seconds
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: fe26549 and then dc20c76, 2026-09-29, each report measures its window and prints its real length, so the first reads from attach. fe26549 had named the first window in words, and dc20c76 replaced that with the measure when H-22 found the later windows wrong too.

`src/engine/exceptions.cpp:147` to `:150`. The counts start at attach, and the ten ticks come from the
timeline thread, which starts at the first `DStorageGetFactory`, 2.8 s later in
`logs/clean-laps-20260916`. No session on disk has logged a throw. F-06's shape in the engine angle's
file, taken here since that angle has merged. Older, 3c87596.

Batch 2's run raised one more, read from its log when the launch was checked.

### V-22: the per thread table ranks threads by their samples outside wait, so threads parked in a lock outrank a busy worker held for less of the window, and a short load's workers fall off the printed sixteen
- severity: debt
- found-by: verifier
- batch: 3
- status: fixed
- fix: 99f8a12, 2026-09-29, the table ranks threads by the samples they were running, outside wait and lock.

`src/telemetry/load_sampler.cpp:335` sorts on `total - bucket[kWait]` and `:337` prints sixteen. A
thread parked on a condition variable or a lock sits in `NtWaitForAlertByThreadId`, which files as
lock, so it ranks as if busy. In the load window of `logs/telemetry-b2-20260929`, t+30, four D3D
Background Threads at 99.5 percent lock took rows with 219 samples each, Resource Manager Worker 4 at
49.2 percent game code took the last row with 191, and none of the other eight workers the boost adds
printed. The pick now holds the right threads for the right stretch, and the printout hides them.
Older, 50b195f.

Two hunters then ran on batch 3, one on the frames and timeline fixes and one on the sampler and throw
log fixes. They found each fix right on the paths it names, F-06 lighter than filed as recorded above,
and no tool that reads these files thrown by the change, the docs being H-23's, and raised nine. H-17
is the one bug, the hitch line still counting across a load, which 67733f5 left standing. H-18 and
H-19 are the fixes' own comment and timing, H-20 to H-22 older labels in the sampler and the throw log,
and H-23 to H-25 records the fixes made stale or miscounted.

### H-17: the first hitch line after a pause over 2 s still counted across the pause, where 67733f5 said it no longer did
- severity: bug
- found-by: hunter
- batch: 3
- status: fixed
- fix: 23cdc04, 2026-09-29, the hitch line starts counting again at the present after a pause, as it does at a chain's first present.

`src/render/frame_stats.cpp:91`. The pause return came before the hitch baseline moved, so the next
hitch reported everything since the last hitch before the load. In `logs/lap10-sampler-20260905-2219`
the 1343.9 ms hitch at t=29.94 reports 32,469 requests since the hitch at t=20.94, about 25,000 of them
in the 7 s with no present, and 128 of the 158 pauses in the saved frames CSVs end in a frame over the
default hitch_ms of 33, so nearly every load's first hitch line carried the whole load. Older, 0140743,
left standing by 67733f5.

### H-18: the timeline's new comment said every counter's baseline is read together, where the six counters each tick zeroes were not
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: aeeaf3d, 2026-09-29, those six are zeroed where the baselines are read.

`src/telemetry/timeline.cpp:81`. `g_frames`, `g_frameSumUs`, `g_frameMaxUs`, `g_hitch20`, `g_hitchCfg`
and `g_tileBatchMax` reached back to when they started counting, though on 0.9.1 none has counted
anything by then. ef1556a wrote it.

### H-19: the sampler's first summary counted the thread's setup as CPU used in its window
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: a3994ee, 2026-09-29, the baseline is read where the window starts.

`src/telemetry/load_sampler.cpp:62`. Up to about 75 ms of the 812 ms batch 2's run gives its first window
was the module and export tables and the first pick, all before the window opened. Raised by both
hunters. 6a34f99 caused it.

### H-20: the per thread table keyed threads by the name read at each refresh, so the game thread, named late, made two rows in the first window
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: d12ee4d, 2026-09-29, the table counts by thread id and prints the last name seen, so a target keeps its id again, which H-13 had removed as unread.

`src/telemetry/load_sampler.cpp:439`. In `logs/telemetry-b2-20260929` t+15, "tid 6444" at 495 samples
and GameThread at 1214 are one thread, and every run with a per thread table shows the same unnamed row
in its first window. 99f8a12 moved that row from ninth to fourth. Older, 1024e7e.

### H-21: the sampler's CSV and summaries counted seconds from the sampler's own start, where every other file counts from attach
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: 64e55ad, 2026-09-29, both count from attach.

`src/telemetry/load_sampler.cpp:453`. The sampler starts at the first `DStorageGetFactory`, 2.8 to 5.5 s
after attach in the saved runs, so a row joined to the frames CSV or the trace by its time landed that far
early, more than a third of a Red Bull Ring load. Older, 1024e7e.

### H-22: the throw log's later reports said the last 10 s for ten timeline ticks, which a memory census in the same thread stretches
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: dc20c76, 2026-09-29, each report measures its window and prints its real length, which covers H-16's first window too.

`src/engine/exceptions.cpp:147` and `:162`. A census held the timeline thread for 9.5 s in
`logs/airace-hang-20260918`, so a window holding one would cover about 19.5 s and halve any rate read
from it. It needs both developer switches on, and no session on disk has had both. Older, 3c87596.

### H-23: two knowledge docs still said the vram columns come from the discrete adapter
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: 4f99298, 2026-09-29.

`.agent/docs/ops/telemetry.md:82` and `.agent/docs/foundation/proxy-architecture.md:142`. 3c7b742 made it
false for a machine whose only adapter reports no dedicated memory.

### H-24: two counts in this ledger's batch 3 text did not match the saved runs
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: 2026-09-29, 98 of 99 runs with the timeline on, counting the nested sessions, and the start up burst beginning within the first three rows. The first correction said within them, which the verifier found a row short, since `logs/ui-probe-a-20260914` spills the burst's last 12 submits into row four.

cbfe28c wrote them.

### H-25: the agent directory ledger's F-10 still asked for a note on the sampler CSV's fifteen second reset, which cb7fccf removed
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: 2026-09-29, F-10 now says each row holds its own second and counts from attach.

`.agent/reviews/2026-09-sweep-review-agent-dir/ledger.md:178`. cb7fccf made it stale.

Two verifiers then ran on batch 3, one on the frames, timeline and throw log code and one on the
sampler and the records. They found every finding closed but H-24, whose corrected count was still a
row short, the zeroing and the new baselines safe against the present thread, the two new timeline
lines true wherever they can print, and the head built from every change, and raised five. V-23 is the
hitch line's label and the doc beside it, which 23cdc04 and 67733f5 left saying less than the code
does. V-24 is H-20's key merging two threads that share an id, V-25 an older name buffer the same
change copies, and V-26 and V-27 are records.

### V-23: the hitch line and the telemetry doc said the count runs from the previous hitch, where after a pause it runs from the present that ended it
- severity: nit
- found-by: verifier
- batch: 3
- status: fixed
- fix: 486a4b7, 2026-09-29, the line says since previous hitch or pause, and the doc says an unlogged hitch or a pause over 2 s restarts the count.

`src/render/frame_stats.cpp:100` and `.agent/docs/ops/telemetry.md:45`. The first hitch after every load
counted from a present the log never shows, while reading as if it counted from the hitch line above
it, and 176 of the 1300 lines in `logs/ui-probe-a-20260914` count from a hitch the budget kept out of
the log. 23cdc04 caused the pause case, and 67733f5 the unlogged hitch.

### V-24: keyed by thread id alone, the per thread table put a new thread given an exited one's id into that thread's row
- severity: nit
- found-by: verifier
- batch: 3
- status: fixed
- fix: 287f839, 2026-09-29, the row is keyed by the thread's id and creation time, the one the pick already reads.

`src/telemetry/load_sampler.cpp:445`. The row printed under the new name with both threads' shares
mixed, where keying by name had kept two differently named threads apart. Threads do exit during a
load, as the unreadable samples in the load windows of `logs/telemetry-b2-20260929` show. d12ee4d caused
it.

### V-25: a thread name of 40 or more bytes was left in the buffer unterminated
- severity: nit
- found-by: verifier
- batch: 3
- status: fixed
- fix: 5d4ffd6, 2026-09-29, the name is converted whole into a larger buffer and then cut to fit.

`src/telemetry/load_sampler.cpp:240`. `WideCharToMultiByte` fails on a buffer too small with the buffer
full and no terminator, so the table would print on past the name. No sampled name in a saved run is
longer than 26 characters, and d12ee4d copies the same buffer into each row. Raised in passing by the
verifier. Older, 1024e7e.

### V-26: three details in the batch 3 hunter text did not match the ledger, the history or the file
- severity: nit
- found-by: verifier
- batch: 3
- status: fixed
- fix: 2026-09-29. The hunter paragraph no longer says no doc was thrown, since H-23 counts two, H-20 is dated to 1024e7e, which first keyed the table by name, and H-20 and H-21 cite `:439` and `:453`.

7c3f581 wrote them.

### V-27: two lines the batch 3 records rewrote were left unwrapped
- severity: nit
- found-by: verifier
- batch: 3
- status: fixed
- fix: 2026-09-29, both paragraphs rewrapped.

The batch 2 run paragraph of this ledger, where H-19's clause went in, and
`.agent/docs/foundation/proxy-architecture.md:142`. 7c3f581 and 4f99298 left them.

A second, shorter verifier round checked V-23 to V-27 and H-24. It found all six closed, V-25's cut
replayed on the real conversion at 39, 40 and 255 bytes, and the report tool reading all 1300 hitch
lines of a run rewritten to the new wording, and raised three. The loop stopped there, with only
record precision and one comment left.

### V-28: V-25 cited a line only an intermediate commit has
- severity: nit
- found-by: verifier
- batch: 3
- status: fixed
- fix: 2026-09-29, `:240`, the line at the base the verifiers read, like V-24 beside it.

185e0d7 wrote it.

### V-29: the batch 3 verifier paragraph filed V-23 among the records, where it is the hitch line's label and was fixed in code
- severity: nit
- found-by: verifier
- batch: 3
- status: fixed
- fix: 2026-09-29.

185e0d7 wrote it.

### V-30: the comment 486a4b7 rewrote beside the hitch baseline still left the pause out
- severity: nit
- found-by: verifier
- batch: 3
- status: fixed
- fix: 30239df, 2026-09-29.

`src/render/frame_stats.cpp:103`. 486a4b7 wrote it.

### F-04: heap handles are used well after the snapshot, and each swallowed fault costs the game 120 to 210 ms plus a crash report naming the mod
- severity: bug
- found-by: review
- batch: 4
- status: fixed
- fix: 39ff642, 2026-09-29, each heap is looked for in the live heap list just before it is read or compacted, and one no longer there is skipped. A heap destroyed between that look and the call can still fault, which leaves microseconds where there were milliseconds.

`src/telemetry/memory_census.cpp:199`. `ReadHeaps` snapshots handles at 195 and then calls
`HeapSummary` on each, `CompactHeaps` does the same at 176 and 177, and a census does three such passes
over every heap with one pass alone taking 1.4 to 4.2 seconds.

Failure: a component unloads during that window and destroys its heap, and the next `HeapSummary` or
`HeapCompact` faults on a destroyed handle. The `__try` at 158 and 167 makes that safe in the sense that
the census continues, but in this game a handled access violation is not free. The crash logger stalls
the faulting thread and writes a line naming DSTORAGE.dll, which is BUG-022's shape, and here it can
fire once per heap per pass. A census that faults on five heaps costs an extra second and puts five
bogus crash reports in the player's game log.

The window was smaller than filed, V-33. On this Windows HeapSummary on a destroyed heap returns FALSE
and raises nothing, so the two long read passes never faulted on one. Only HeapCompact does, and its
pass takes its own list and ran in 0.00 s in all 20 compaction passes in the saved census CSVs, so the
window was milliseconds.

### F-07: GetProcessHeaps is called twice with sixteen slots of slack, and overflowing that reads every slot as a null heap handle
- severity: debt
- found-by: review
- batch: 4
- status: fixed
- fix: 65a8b54, 2026-09-29, one helper reads the list, and when it returns a count larger than its buffer the buffer grows past it and the call goes again.

`src/telemetry/memory_census.cpp:176`. Line 175 sizes the vector to the count plus 16 and line 176 fills
it. When the true count exceeds the buffer, `GetProcessHeaps` stores nothing and returns the real count.
The code clamps with `std::min` and then iterates a vector still value initialised to nullptr.

Failure: `ReadHeap(nullptr)` and `CompactHeap(nullptr)` fault on every slot, each fault another crash
logger stall by F-04, so a 60 heap process would stall about nine seconds and write sixty bogus crash
reports, and it silently reports zero heaps committed. Sixteen new heaps inside the microseconds between
the two calls is unlikely, and the failure mode is loud. A retry loop is the fix.

This Windows does otherwise, V-32. With a buffer too short it stores the list's first handles and
returns the whole count, so the old code left the heaps past its buffer out of the totals and the
compaction rather than reading nulls, and HeapSummary(NULL) returns TRUE here where only
HeapCompact(NULL) faults. The documentation says nothing is stored, and the retry covers both.

### F-08: the pending buffer's capacity is thrown away on every flush, so the append path re-doubles from zero each second under the exclusive lock
- severity: debt
- found-by: review
- batch: 4
- status: fixed
- fix: 2af9195, 2026-09-29, two buffers are traded at each flush and the written one is cleared, so the buffer the rows go into keeps the capacity it grew to. Each can hold up to the 64 MB cap for the rest of a run once a stalled writer has let it grow that far.

`src/telemetry/streaming_trace.cpp:66`. `TraceFlush` declares a fresh empty string and swaps it with
`g_pending`, so `g_pending` comes back with no capacity and the old buffer is freed.

Failure: at a few MB a second of rows the producers rebuild the buffer from scratch every second, about
20 reallocations each copying the whole buffer so far, and every one of them happens inside
`AcquireSRWLockExclusive` at line 39 with every streamer and DirectStorage hook in the process blocked
behind it. If the writer falls behind, which a memory census freezing the timeline thread does, the
buffer walks toward its ceiling and the last doubling copies tens of megabytes under that lock.

The rate was overstated about tenfold, H-31. The saved traces run 6 to 27 KB a second on average and
153 to 276 KB in their busiest second, so the regrowth copied a few hundred KB a second, and tens of
megabytes would take minutes without a flush.

### F-10: the enabled check lives inside TraceRow, so callers still evaluate their arguments when the trace is off
- severity: nit
- found-by: review
- batch: 4
- status: fixed
- fix: 7645d8d, 2026-09-29, the kick row checks `TraceOn()` before its arguments are read, as the other streamer rows already did.

`src/telemetry/streaming_trace.cpp:25`. `TraceRow` is variadic and tests the flag as its first statement,
so at `streamer.cpp:521` every streamer kick still evaluates seven raw offset reads into the engine's
streamer and kick structs plus `GateSpace(frame)` before calling a function that discards them. The cost
is small, and those are raw offset reads into engine memory on a code path that is supposed to be
entirely off, which is the fault surface BUG-022 came from. The other call sites guard properly with
`TraceOn()` at streamer.cpp:550, 596 and 652, or at install time in texture_writes.cpp:216.

Batch 3's run raised one more, read from its log when the launch was checked.

### V-31: the per thread table can print two rows under one name with nothing to tell them apart, since the game runs threads that share a name
- severity: nit
- found-by: verifier
- batch: 4
- status: fixed
- fix: 5e114af, 2026-09-29, each row prints the thread's id beside its name.

`src/telemetry/load_sampler.cpp:364`. The game rebuilds its render and resource workers each time the
loading boost goes on or off, keeping their names, and during a load it runs a second Resource Manager
Worker 0 beside the first. Keyed by id and creation time, each thread now has its own row, which is
right, and the old name key had merged a boosted worker into a resting one. But in the first window of
`logs/telemetry-b3-20260929` Render Workers 0, 1, 3 and 4 and Resource Manager Workers 0 and 1 print
twice each, and its load window prints two Resource Manager Worker 0 rows, 794 samples at 46 percent
wait and 48 at 54 percent in the job queue spin, with nothing to say which is which. Printing the thread
id beside the name would. d12ee4d and 287f839 brought the rows apart.

Two hunters then ran on batch 4, one on the census and one on the trace, the streamer guard and the
sampler's id column. They found every fix right on the paths it names, the heap check leaving
microseconds on the heaps that can be destroyed while the process heap, the one that takes seconds,
cannot be, no row written twice or lost that was not lost before, and no census run on disk with a
crash report, and raised six. H-28 and H-29 are F-10's and F-08's shapes elsewhere, the streamer's want
and drop events and the frames buffer. H-30 is the id column's own, and H-26, H-27 and H-31 are
comments and records.

### H-26: the comment above the census's heap calls said a fault on a heap only skips it
- severity: nit
- found-by: hunter
- batch: 4
- status: fixed
- fix: 438b506, 2026-09-29, it says the __try keeps the census going but a fault still stalls the thread and writes the crash report, and that touching an unserialised heap in use is a risk the census still takes. The heap list comment now says the buffer grows past the count, as the code does.

`src/telemetry/memory_census.cpp:154`, against the comment 39ff642 put on StillAHeap, which says every
such fault stalls the thread and writes a crash report naming the mod. Older, 251667c and 0770d67,
both written before BUG-022 was known.

### H-27: the census header said the tick writes its CSV every stats interval, where it writes only when a census runs
- severity: nit
- found-by: hunter
- batch: 4
- status: fixed
- fix: c8570c2, 2026-09-29.

`include/acevo/telemetry/memory_census.h:11`. Nothing in the census reads `stats_interval_s`, and
`logs/sessionfix-rbr-20260916/acevo_perf_memory.csv` has rows at 23.7, 233.5, 361.6 and 485.0 s. Older,
251667c, left standing when 0770d67 moved the census to the settled trigger.

### H-28: the streamer's want and drop events read the engine for columns only the trace uses
- severity: nit
- found-by: hunter
- batch: 4
- status: fixed
- fix: 6a41e3f, 2026-09-29, a want reads its feedback only for a reload, which the flip test needs, or for the trace, and a drop sums its tiles only for a refusal or the trace. Every decision is unchanged, since a flip needs a reload and the tiles feed only the refused tally and the row.

`src/engine/streamer.cpp:595` and `:639`, F-10's shape at two more sites, whose trace calls were guarded
while the reads that fill them were not. Both run in every default session, and in
`logs/ai30-A-fix-on` 92 percent of want rows are no reload, about 185 a second, and 7,698 of 8,257
drops are not refused. The memory is what the engine's own getters read in the same kick, so this was
work and no added fault. Older, 6ff06bb and 9912329.

### H-29: the frames buffer was thrown away every second, so the present hook regrew it under g_frameCs
- severity: nit
- found-by: hunter
- batch: 4
- status: fixed
- fix: 5308536, 2026-09-29, the timeline keeps the drained buffer across ticks and trades it back cleared.

`src/telemetry/timeline.cpp:147`, growing at `src/render/frame_stats.cpp:111`. F-08's shape on the
presenting thread, about a dozen allocations and copies a second under the lock at 60 to 144 fps, with
`frames=1` only. Older, 57b43fb and de460b9.

### H-30: an unnamed thread's row printed its id twice
- severity: nit
- found-by: hunter
- batch: 4
- status: fixed
- fix: fdfae6a, 2026-09-29, a thread with no name reads (unnamed), and the id stays in its own column.

`src/telemetry/load_sampler.cpp:252`. NameThread fell back to the name "tid N", and the row now adds its
own id beside it. 5e114af caused it.

### H-31: F-08 overstated the trace's rate about tenfold, and V-31 had Render Worker 2 printing twice
- severity: nit
- found-by: hunter
- batch: 4
- status: fixed
- fix: 2026-09-29, a note under F-08 and V-31's list of workers.

79177bc and f7a4db4 wrote them.

The verifier on the census, trace and frames fixes found all six closed and raised two, tested with
small programs on this machine's Windows. The verifier on the streamer, sampler and records was stopped
unfinished at the owner's usage limit on 2026-09-29 and runs again before the batch 4 launch.

### V-32: the heap list comment and F-07 say a list longer than the buffer fills nothing, where this Windows fills the buffer with the list's first handles
- severity: nit
- found-by: verifier
- batch: 4
- status: fixed
- fix: 46ead16, 2026-09-29, the comment says a short buffer holds part of the list, the first handles here and none by the documentation, and F-07 carries a note and "past" in its fix line.

`src/telemetry/memory_census.cpp:174` to `:176`, and F-07's failure text and fix line. With 64 slots
for 102 heaps GetProcessHeaps returns 102 and leaves the first 64 handles, so the old code left heaps
out rather than reading nulls, and HeapSummary(NULL) returns TRUE here where only HeapCompact(NULL)
faults. The retry still fixes the missed heaps. 65a8b54 and 438b506 wrote the comment, 79177bc F-07's
text.

### V-33: F-04 and the StillAHeap comment treat the seconds long census as the fault window, where here only HeapCompact faults on a destroyed heap and its pass takes milliseconds
- severity: nit
- found-by: verifier
- batch: 4
- status: fixed
- fix: 290bfdb, 2026-09-29, the comment names HeapCompact as the call that faults, and F-04 carries a note, milliseconds in its fix line, and the hunter paragraph no longer gives seconds as the window.

`src/telemetry/memory_census.cpp:190` to `:193`, and F-04's failure text, its fix line and the batch 4
hunter paragraph. HeapSummary on a destroyed heap returns FALSE with error 87 and raises nothing, and
all 20 compaction passes in the saved census CSVs took 0.00 s. The check does no harm in the read
passes. 79177bc, 0d1baca, fe2d3ce and 39ff642 wrote it.

The verifier on the streamer, sampler and records then ran again, with V-32's and V-33's fixes added.
It found all seven closed, every streamer decision the same as before 6a41e3f and 7645d8d on every
path in the source and the object code, and no read that can now fault where it did not, and raised
two, both records. The loop stopped there, with only record precision left.

### V-34: four details the batch 4 records wrote did not match the runs or the text they describe
- severity: nit
- found-by: verifier
- batch: 4
- status: fixed
- fix: 2026-09-29. F-04's note counts 20 compaction passes rather than 20 CSVs, H-28 gives ai30's want rate as about 185 a second, V-33's fix line says seconds are gone as the window, and the batch 4 hunter paragraph is rewrapped.

There are four saved census CSVs, and the 20 are compaction passes in three of them. ad267fe wrote three
of them and fe2d3ce the rate.

### V-35: two older numbers in this ledger were off
- severity: nit
- found-by: verifier
- batch: 4
- status: fixed
- fix: 2026-09-29, F-10's kick row made seven raw offset reads plus GateSpace's two, and the batch 3 run's boosted workers ran 69 to 85 percent game code, Resource Manager Worker 10 at 68.8.

79177bc and f7a4db4 wrote them.

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

Batch 2, one launch on 2026-09-29, `logs/telemetry-b2-20260929`, 13:28 to 13:30, the menu, a load into
Oulton Park and about half a minute on track, with `load_sampler=1` for this launch alone, on 5ba0a6b's
build. The sampler used 812, 844, 781 and 719 ms of CPU in its four 15 s windows, the first including
up to 75 ms of its own setup, H-19, where the old loop held a whole logical processor, about 15,000 ms,
and it still took about 960 samples a second. While driving it held about 17 threads at a time where
the old pick always held 24, the render workers, GameThread, Physics and its six workers and the audio
mixer among them, and the parked D3D Background Threads the old pick kept in full slots no longer held
one. In the load window Resource Manager Worker 4 printed at 49.2 percent game code and 37.7 percent in
the job queue spin. Every ntdll label was an Nt name, `NtDelayExecution` and
`NtWaitForWorkViaWorkerFactory` among them. No timer failure line, a clean `detached` and no
`Exception Detected`. H-12's reused id cannot show in a run.

Batch 3, one launch on 2026-09-29, `logs/telemetry-b3-20260929`, 16:46 to 16:49, the menu, a load into
Oulton Park and about half a minute on track, with `timeline`, `frames` and `load_sampler` on for this
launch alone, on 34dc0f2's build, whose code differs from the close only by V-30's comment. The frames
CSV's first row and the first `[hitch]` line both carry 399 file to memory requests where about 8,900
used to land, and the timeline's first row carries the start up burst of 107 submits beside its own 12
tile requests. The timeline found the RTX 3060. The sampler's rows hold about a thousand samples each
with no sawtooth, its t_s and the timeline's read 4.7 on the same second, and its summaries count t+
from attach. Its tables print GameThread once, the unnamed rows being threads that never take a name,
and in the load window seven workers the boost brings up, at 69 to 85 percent game code and 50 to 62
percent in the job queue spin, while the parked D3D Background Threads take no row. The sampler used
547 to 953 ms of CPU a window. This load never paused over 2 s, its longest frame 1943.5 ms, so H-17
could not show, and with the throw log off neither could H-16 and H-22. A clean `detached` and no
`Exception Detected`.

Batch 4, one launch on 2026-09-29, `logs/telemetry-b4-20260929`, 18:57 to 19:01, the menu, a load into
Oulton Park, about a minute and a half on track, the menu again and a quit, with `memory_census`,
`load_sampler` and `frames` on for this launch alone and the trace off, so the streamer ran the path
players run, on the code of the close. The census ran at start and after the unload, reading the heaps
in 0.29 and 0.54 s at 3738 and 4234 MB committed and compacting them in 0.00 s, and the game log has
no `Exception Detected`. With the trace off the streamer refused 90 drops holding 237 MB and pinned 197
flips, so the reload path still reads the feedback it needs. The sampler's tables print each thread's
id, two Resource Manager Worker 1 rows telling apart as 105684 and 106972, its unnamed threads read
(unnamed), and it used 641 to 1156 ms of CPU a window. The frames CSV holds 17,698 rows. A heap
destroyed mid census and a heap list outgrowing its buffer cannot be made to happen in a run, and the
trace's kept buffer changes nothing a file shows. A clean `detached`.

## Checked and clean

The suspected heap lock inversion in memory_census.cpp is not reachable, because ntdll's heap commits
through `NtAllocateVirtualMemory` rather than the import patched `VirtualAlloc`, so nothing ever holds
the heap lock and then waits on ours. The `TraceRow`, `WriteRow` and `Log` buffer arithmetic is correct
at every boundary the reviewer could construct, including the `_TRUNCATE` return of -1.

The census freeze itself, several seconds on a large heap, is documented and deliberate and is not
re-filed here.
