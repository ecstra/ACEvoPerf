---
name: review-2026-09-sweep-review-proxy-core
kind: review
description: the proxy and core angle of the full review of main, an off switch that takes the override layer with it and a row of ini values used without validation, fifteen findings, one breaks
updated: 2026-09-20
links: [spec-reviews, house-rules-agent, proxy-architecture, reviews-index]
branch: sweep/review-proxy-core
status: open
---

# Review of the DirectStorage proxy and the core

## Summary

One angle of the full review of main run on 2026-09-20 at high. This angle covers
`src/dstorage/proxy.cpp`, `src/dstorage/stats.cpp`, `src/core/config.cpp`, `src/core/iat.cpp`,
`src/core/code_patch.cpp` and `src/dllmain.cpp`. The teardown path in `src/core/log.cpp` belongs to
`fix/review-shutdown` and is not repeated here.

The COM layer itself came back clean, which is the part that would have been most expensive to get
wrong. What the review found instead is a row of values that cross the ini boundary and are used
without a single check, and one switch whose name promises far less than it does.

Fifteen findings, one breaks, six bug, four debt, four nit.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | an off switch turns off only what it names | pending | |
| 2 | every value that crosses the ini boundary is validated | pending | |
| 3 | one time init happens once, and a freed object is not left addressable | pending | |
| 4 | the log does not carry the player's machine into a public post | pending | |
| 5 | the leftovers | pending | |

## Findings

### F-01: turning stats off silently switches the whole package override layer off with it
- severity: breaks
- found-by: review
- batch: 1
- status: open
- fix:

`src/dstorage/proxy.cpp:350` wraps a queue in `QueueProxy` only when
`(g_cfg.stats || g_cfg.logRequests || g_cfg.streamingTrace)`. The two developer flags ship off.
`QueueProxy::EnqueueRequest` at line 274 is the only caller of `OverlayRedirect` anywhere in the tree.

Failure: a player sets `[directstorage] stats=0` to quieten the log. The trackside screen fix
(BUG-017), the responsive UI stylesheet override and every loose file in acevo_mods stop being served,
while `overlay::Install` still logs "file hooks installed" and "table rebuilt", so the log reads
healthy and the only tell is the absence of a redirect line.

Verified by me against the source on 2026-09-20, `grep -rn OverlayRedirect src/` gives exactly one call
site.

### F-02: an empty overlay folder value makes the whole game folder the mods folder
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`src/core/config.cpp:105`. `GetPrivateProfileStringW` returns the default only when the key is absent,
not when it is present and empty, so `folder=` yields an empty string. `overlay.cpp:574` then builds
`folder = g_dir`, which is a real directory.

Failure: `CollectFiles` recurses the entire game install from DllMain under the loader lock and adds
every file it finds as an override, content.kspkg and acevo_perf.log included. Start up stalls before a
window is ever shown, and the rebuilt package table carries thousands of entries that have nothing to
do with the package. `CollectFiles` also has no depth limit, so a directory junction loop recurses
until the stack ends.

### F-03: the megabytes to bytes conversion overflows UINT32, and 4096 lands on exactly zero
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`src/dstorage/proxy.cpp:377`, and the same expression again at line 457. A player with a 24 GB card
writes `staging_buffer_mb=4096`, and `(UINT32)4096 * 1048576u` wraps to 0. dstorage.h defines
`DSTORAGE_STAGING_BUFFER_SIZE_0` as "there is no staging buffer", with requests that exceed the
specified size failing.

Failure: every DirectStorage request in the game fails. The log line at 379 prints "applied 0 MB" and
the call returns S_OK, so nothing reads as an error. Nothing clamps `stagingMb` on the way in.

### F-04: staging_buffer_mb is matched against "auto" without lowercasing, unlike every other string option
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`src/core/config.cpp:44`. `tile_queue_priority`, `priority` and `gpu_priority` all lowercase their value
first. This one does not.

Failure: a player writes `staging_buffer_mb=Auto`, or clears it to `staging_buffer_mb=`. The compare
fails, `stagingAuto` goes false, `_wtoi` gives 0, and both `> 0` gates at `proxy.cpp:377` and `:456`
are dead, so the 128 MB staging cap never lands and the game runs with its own staging size. That is
what BUG-003, BUG-004 and BUG-005 traced to video memory exhaustion. The log prints "staging=0MB" and
says nothing else.

### F-05: stats_interval_s and sample_us are unvalidated, and zero is catastrophic for both
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`src/core/config.cpp:55` and `:117`. `IniInt` returns `_wtoi` unfiltered.

Failure with `stats_interval_s=0`, which a player could reasonably read as off: the test at
`proxy.cpp:204` becomes `dt < 0`, never true, so `Report` writes its roughly 400 byte line on every
`QueueProxy::Submit`. During a Nurburgring load that is thousands of blocking `WriteFile` calls a
second on the game's submit threads, through the same critical section the render thread uses. A
negative value converts to a huge unsigned and turns stats off in silence instead.

Failure with `sample_us=0`: `load_sampler.cpp:354` computes an interval of 0, the wait loop at 365
exits immediately, line 373 resets the deadline to now, and a `THREAD_PRIORITY_HIGHEST` thread
suspends and resumes the game's busiest threads with no delay at all. The only way out is killing the
process. That key is in neither the shipped ini nor the telemetry doc, so a user who sets it has no
guidance.

The codebase already knows these need bounds. `minQueueCapacity` is clamped to the DirectStorage
minimum and maximum at `proxy.cpp:337` to 340, at the use site rather than at the read.

Found independently by three reviewers.

### F-06: the one time late init block sits outside the lock the same function takes eight lines later
- severity: bug
- found-by: review
- batch: 3
- status: open
- fix:

`src/dstorage/proxy.cpp:443`. `g_configApplied` and the plain `static bool lateApplied` carry no guard.

Failure: two engine subsystems ask for the factory on their own threads during start up and both read
both flags as false. `ApplyFlags("late")` runs twice, `StartLoadSampler` runs twice, and
`StartTimeline`'s `if (g_timelineThread) return` loses the race, so two timeline threads come up. The
second one's `CreateFileW` on the CSV fails with a sharing violation, so it runs the per second tick
with no CSV, double refilling the hitch budget and double driving the throw log, and one thread handle
leaks.

### F-07: the log the readme tells players to post publicly records the game's full command line and install path
- severity: bug
- found-by: review
- batch: 4
- status: open
- fix:

`src/dllmain.cpp:62` writes the full command line from `GetCommandLineW` and line 54 writes the full
ini path. README.md:71 and dist/README.txt both ask the player to attach acevo_perf.log when reporting
a problem.

Failure: a player whose library sits under their profile posts a path carrying their own name to a
public forum. The command line is the worse half, because a launcher that passes authentication
arguments puts them in a file users are told to attach. Logging the leaf name plus the argument count
keeps the diagnostic value.

### F-08: FactoryProxy::Release deletes the object without clearing g_factory
- severity: debt
- found-by: review
- batch: 3
- status: open
- fix:

`src/dstorage/proxy.cpp:327`. Latent today, because the constructor's own reference means a well
behaved game never drives the count to zero.

Failure: one extra Release from the game, or two subsystems releasing a factory they obtained once,
takes the count to 0, frees the object and leaves `g_factory` pointing at it.
`RealDStorageFactory()` at line 390 then reads freed memory on every overlay redirect and hands it to
`OpenFile`, and the next `DStorageGetFactory` returns the same dead pointer. Clearing `g_factory`
inside the same critical section the creation uses is the whole fix.

### F-09: the repeated read counters are process wide but printed as if they belonged to one queue
- severity: debt
- found-by: review
- batch: 5
- status: open
- fix:

`src/dstorage/proxy.cpp:216`, with the counters at line 141 at file scope. With several file source
queues, each queue's stats line prints the process wide total under its own name, and the measurement
TODO-018 rests on reads as though one queue did all of it. The `g_reads` map behind it is never cleared
and is keyed on a raw `IDStorageFile*`, so once the game closes a file and the allocator hands the
address back, the old entries count fresh reads of a different file as repeats.

### F-10: WriteCode copies over live code with a plain memcpy, with no thread suspension and no atomic write
- severity: debt
- found-by: review
- batch: 5
- status: open
- fix:

`src/core/code_patch.cpp:43`. Safe today only because of when it is called. Every caller runs from
DLL_PROCESS_ATTACH while the game is still single threaded.

Failure: the first patch installed lazily from a runtime hook, for instance one armed when a later
module loads, has a game thread execute a half written five byte jump and crash inside the game's own
code with no mod frame on the stack. Nothing in the function signals that constraint.

### F-11: InstallUiProbe creates a thread from DllMain, under the loader lock
- severity: debt
- found-by: review
- batch: 5
- status: open
- fix:

`src/dllmain.cpp:70` with `developer.ui_probe=1` creates the layout sampler thread at
`ui_probe.cpp:1252` while DLL_PROCESS_ATTACH holds the loader lock. The new thread cannot start until
DllMain returns, because its own thread attach pass needs that lock. It works today, and
`StopLoadSampler`'s comment shows the detach side of the same rule was thought about, but any DLL in
the process whose thread attach handler blocks would hang the launch. The responsive UI gets this right
by creating its thread from `OnLibrary` instead.

### F-12: GetModuleFileNameW's result is used without checking for truncation
- severity: nit
- found-by: review
- batch: 5
- status: open
- fix:

`src/dllmain.cpp:42`. Past MAX_PATH the buffer is truncated and still null terminated, and the return
value is dropped, so `g_dir` names an ancestor of the real folder, the ini is not found, and the
absolute load of dstorage_orig.dll fails into the error message box. Needs a game installed under a
path over 260 characters. One comparison against `ERROR_INSUFFICIENT_BUFFER`.

### F-13: the header comment says no game files other than dstorage.dll are touched, and the overlay writes two
- severity: nit
- found-by: review
- batch: 5
- status: open
- fix:

`src/dllmain.cpp:16`. `AddBigScreenFix` and `AddUiStyleFix` create acevo_bigscreen.texture and
acevo_uicomponents.css in the game folder on every start, both on by default. dist/README.txt already
tells players to delete everything starting with acevo_, so only the source comment is stale.

### F-14: every vtable hook in the mod logs a DXGI prefix, including the ones that have nothing to do with DXGI
- severity: nit
- found-by: review
- batch: 5
- status: open
- fix:

`src/core/iat.cpp:71` hardcodes `Log("DXGI: hooked %s", what)` and `HookVtableSlot` is the single shared
vtable patcher with thirteen call sites across dxgi_hooks, frame_stats, cohtml_hooks, responsive_ui and
ui_probe. The shipped logs therefore read "DXGI: hooked Cohtml Library::ExecuteWork". Anyone reading
acevo_perf.log to check a UI fix landed is told it came from the DXGI layer.

### F-15: dt means elapsed milliseconds at one line and a destination type at another, in the same class
- severity: nit
- found-by: review
- batch: 5
- status: open
- fix:

`src/dstorage/proxy.cpp:205` and `:260`. `QueueProxy::Report` also uses single letter locals where the
rest of the codebase names things, and packs two statements per line at 207 and 218.

## Checked and clean

`src/exports.def` and the export forwarding table. The export directories of dist/dstorage.dll, the
Microsoft original and dstorage_orig.dll all carry the same four names at the same ordinals 1 to 4, so
the ordinal path resolves identically and nothing is missing or misspelled. acevo_dstoragecore.dll
exports the three Core entry points plus `DStorageSDKVersion` that `LoadBundledCore` looks up, and
DEC-015's naming is consistent throughout.

COM method coverage. `QueueProxy` implements all nine `IDStorageQueue` methods plus `EnqueueSetEvent`
and `GetCompressionSupport`, `FactoryProxy` all five of `IDStorageFactory`, every one marked override,
so the compiler already enforces completeness. The `IDStorageQueue3` decline at proxy.cpp:234 is
correct and deliberate. AddRef and Release balance in `CreateQueue` and in the unknown IID
passthroughs.

Log format strings. Every `Log` call in the tree passes a string literal as the format, including
`Log("command line: %ls", GetCommandLineW())`, so there is no format string hole.

`src/core/iat.cpp` page protection handling. Both `PatchIatByAddress` and `HookVtableSlot` restore the
original protection, the writes are single aligned pointer stores so a concurrent caller cannot see a
torn value, and the `*orig` guard in `HookVtableSlot` is right rather than wrong, since a vtable is per
class and shared by every instance.

A missing acevo_perf.ini is handled correctly, every default holds, including the `[flags]` section
walk, which is safe against a truncated `GetPrivateProfileSectionW`. No `LoadLibrary` and no thread
wait anywhere on the DLL_PROCESS_ATTACH path with the shipped defaults.
