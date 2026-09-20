---
name: review-2026-09-sweep-review-ui-probe
kind: review
description: the UI probe angle of the full review of main, an instrument that can fault while it holds a game thread suspended and that pays its cost in the frames it exists to explain, nine findings
updated: 2026-09-20
links: [spec-reviews, house-rules-agent, telemetry, reviews-index]
branch: sweep/review-ui-probe
status: open
---

# Review of the UI probe

## Summary

One angle of the full review of main run on 2026-09-20 at high. This angle covers
`src/ui/ui_probe.cpp`, the largest file in the project at 1413 lines. It ships disabled and the
reviewer confirmed that disabled really does mean nothing runs, so none of this reaches a player. It
still matters, because this is the instrument every UI measurement in the project was taken with, and
two of the findings mean the numbers it produced are quietly incomplete.

Nine findings, one breaks, two bug, three debt, three nit.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the sampler cannot fault while it holds a game thread suspended | pending | |
| 2 | the instrument does not distort what it measures | pending | |
| 3 | the report tells the truth about what it dropped | pending | |

## Findings

### F-01: the layout sampler can fault while it holds a game thread suspended
- severity: breaks
- found-by: review
- batch: 1
- status: open
- fix:

`src/ui/ui_probe.cpp:562`. `TakeSample` suspends at 579, walks at 580 and resumes at 581. Inside that
window `FramesOf` dereferences an unwound Rsp at 562 and feeds an unwound context to
`RtlVirtualUnwind` at 568, with an `__except` at 570 that exists precisely because the author expects
faults there.

Failure: the sampler suspends a Cohtml layout worker mid prologue, `RtlVirtualUnwind` returns a bogus
Rsp, and the next read lands on an unmapped page. By the project's own memory note the game's crash
logger symbolizes the access violation before any SEH handler runs, holding the sampler for 120 to
210 ms with the layout worker still suspended. That is a 120 to 210 ms frame. If the suspended worker
was inside `RtlAllocateHeap` or `RtlLookupFunctionEntry` and the crash logger needs the same lock to
symbolize, the process hangs.

The file header at lines 21 to 23 claims the sampler never allocates, locks or logs while a game thread
is suspended. This path breaks that on the game's behalf rather than our own, which is why the claim
reads as true.

### F-02: Hook_Invalidate rebuilds the element description on every call, up to 13,974 calls a second, to feed a report of eight lines
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`src/ui/ui_probe.cpp:916`. Every invalidation runs `DescribeNode`, which is an SEH region, a
`_snprintf_s` and up to four 32 character atom walks, then `strlen`, then `Fnv1a64` over the
description, then an exclusive SRW acquire and a strcmp probe, all before anything decides the entry is
worth keeping.

Failure: summing the per kind call counts in the recorded `[ui] invalidations` lines gives 13,974 in
one second, 9,121 of them one kind. At a few hundred nanoseconds a call that is several milliseconds a
second added to the UI thread, landing in exactly the frames the probe exists to explain. The element
pointer and the kind are already in hand and would key the table without building the string at all.

### F-03: once the 256 entry element table fills, every further invalidation walks all 256 entries and is then dropped with no record
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`src/ui/ui_probe.cpp:936`. The probe loop has no else after it, so with no free slot and no match it
falls out after 256 strcmp calls, still holding `g_markLock` exclusive, and the invalidation is silently
missing from the report.

Failure: a settings or vehicle setup page whose elements carry ids gives more than 256 distinct type
plus id plus class strings in a second, which the recorded keys confirm. At the measured call rate that
is 256 string compares per call on the UI thread, and a report that quietly under counts without saying
so.

### F-04: the twelve frame stack capture runs on every big child list invalidation although only the first of each second is kept
- severity: debt
- found-by: review
- batch: 2
- status: open
- fix:

`src/ui/ui_probe.cpp:923`. `RtlCaptureStackBackTrace` is unconditional for kind 0 with 200 or more
marks, and the decision to keep it is made afterwards at line 926 under the lock. The logs show 66 in
one second, so 65 of those stack walks were done and thrown away, each taking the loader's function
table lock on the UI thread in a burst of a frame or two. Reading the count before capturing skips them.

### F-05: past sixteen layout threads registration stops, so those threads are never sampled and each of their layout calls takes a global exclusive lock for nothing
- severity: debt
- found-by: review
- batch: 2
- status: open
- fix:

`src/ui/ui_probe.cpp:665`. `t_layoutSlot` stays at -1 on an unregistered thread, so
`LayoutThreadForThisThread` takes `g_registerLock` exclusive on every call for the rest of the session,
finds the table full and returns null. The game runs a job worker per hardware thread and the logs show
up to 1,705 layout work calls a second, so on a machine with more than sixteen workers the layout
percentages cover only the first sixteen threads with nothing in the log saying so. The `OpenThread`
handles at line 666 are also never closed.

### F-06: ReadChangedSet scans the whole bucket array on every restyle pass even when the set holds fewer than three nodes
- severity: debt
- found-by: review
- batch: 2
- status: open
- fix:

`src/ui/ui_probe.cpp:884`. The loop ends only at three names or the end of the array, and the set size
read one line earlier at 881 is never used to stop early. Hash tables do not shrink, so after one large
page build every later single node restyle in that document pays a full scan of the grown array, at up
to 357 restyle passes a second, on the frame thread, to name roots that are only printed for the at most
eight slow restyles a second.

### F-07: g_unattributed is declared and never written, so the layout report's percentages hide the samples that were thrown away
- severity: nit
- found-by: review
- batch: 3
- status: open
- fix:

`src/ui/ui_probe.cpp:480`. `TakeSample` discards a sample at 582 whenever the walk produced no frames,
which happens whenever the innermost frame sits in a module with no registered unwind table, and
`g_samples` at 584 counts only kept samples. The counter that exists to record that loss is never
incremented and never printed, so a line reading "cohtml+0x3ED9D0 12%" is a percentage of whatever
survived. `kMaxUnwindModules` is 8 and exactly 8 are registered, so any module added later is silently
dropped into the same blind spot.

### F-08: _snprintf_s with _TRUNCATE returns -1, and every accumulation here treats that as a written length
- severity: nit
- found-by: review
- batch: 3
- status: open
- fix:

`src/ui/ui_probe.cpp:616` and `:620`, `AppendClock` at 1274, `AppendAddress` at 1296 and 1298, and
`UiProbeTick` at 1390, 1396 and 1400 all accumulate the return value. The `length > 0` guards keep every
write in bounds, so this is not a memory bug. After the first truncation each later call rewrites the
tail one byte earlier and the line ends garbled with no indication. The longest line produced so far is
811 bytes of 2048, so it has not fired yet.

### F-09: the sampler reads a layout thread's handle without the register lock, with only a relaxed store publishing it
- severity: nit
- found-by: review
- batch: 3
- status: open
- fix:

`src/ui/ui_probe.cpp:671`. `LayoutThreadForThisThread` writes the id and the handle and publishes with a
relaxed store of the count, all inside the lock, while `LayoutSamplerThread` reads the count at 645 and
the handle at 647 with neither the lock nor an acquire. Nothing but x86 store ordering and the compiler
declining to sink a plain store past a relaxed atomic keeps the sampler from seeing a count that
includes a handle it has not been shown. Benign today because the slot is zero filled and
`SuspendThread(nullptr)` fails, so the pairing holds by accident.

## Checked and clean

Off really means off. `InstallUiProbe` returns at line 1232 before it registers a listener, patches any
Cohtml byte or starts the sampler, and `LoadConfig()` runs at dllmain.cpp:47 well before it.
`UiProbeTick` returns at 1359. `UiProbeTakeEndFrameUs` and `UiProbeTakeAdvanceUs` are atomic exchanges
that return 0 and are only called when frames are on. `InstallCohtmlHooks` still installs because the
responsive UI registers its own listeners, but the probe's callbacks are not in those arrays, so the per
frame cost is zero. The only residue is roughly 280 KB of demand zero .bss for the two count tables,
never touched, so it costs address space and not working set.

The probe's six Cohtml patch sites do not overlap the four shipped fixes, and the chained `ExecuteWork`
hooks resolve in the right order with no recursion. The `kMarkStub` assembly is correct on stack
alignment, shadow space and volatile registers.

The view identity assumption at line 1185 is filed against `fix/review-cohtml-build-guard` as its F-02,
because the counter it depends on lives in cohtml_hooks.cpp.
