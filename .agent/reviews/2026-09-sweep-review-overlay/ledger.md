---
name: review-2026-09-sweep-review-overlay
kind: review
description: the package override layer angle of the full review of main, an unlocked lazy build of a 64 MB table reached from a hook in every module, eleven findings, one breaks, plus twenty three from batch 1's hunter and five verifier passes
updated: 2026-09-23
links: [spec-reviews, house-rules-agent, package-override-layer, reviews-index]
branch: sweep/review-overlay
status: open
---

# Review of the package override layer

## Summary

One angle of the full review of main run on 2026-09-20 at high. This angle covers
`src/overlay/overlay.cpp`, the layer that intercepts reads of the game's content package and serves
replacement bytes. It is on by default for every player, because the trackside screen fix and the UI
stylesheet fix are generated from the player's own package and do not need a mods folder.

The shape of the file is a set of Win32 hooks installed into every module, feeding a lazily built
64 MB table held in one process wide vector. The parsing inside that table is careful and provably in
bounds. What is missing is the concurrency discipline around it and a fallback when a redirect cannot
be served.

Eleven findings, one breaks, three bug, five debt, two nit.

Batch 1 added twenty two more of its own and one for batch 2, from its hunter and five verifier
passes, one bug, three debt and the rest nit. The bug was the hunter's, a player's own file for
either asset the mod corrects never reaching the game. Two were code slips of the batch's own
fixes, V-01 and V-03, and most of the rest were slips in this ledger's and the doc's own wording.
The fifth pass found nothing false and the loop stopped there.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the table is built once and published safely | closed, runtime confirmed | 2026-09-23 |
| 2 | a redirect that cannot be served fails visibly | fixing | 2026-09-23 |
| 3 | the slot layout and the leftovers | pending | |

## Findings

### F-01: BuildToc is an unlocked lazy initialiser of a 64 MB shared vector, reached from a ReadFile hook installed in every module
- severity: breaks
- found-by: review
- batch: 1
- status: fixed
- fix: d05b42b, 2026-09-23, the build runs under `std::call_once` so a second reader waits for it, and `g_tocBuilt` is an atomic stored last.

`src/overlay/overlay.cpp:381`. `Hook_ReadFile` runs on whatever thread reads a tracked package handle,
and `PatchTableRead` calls `BuildToc` with two plain bools as the only guard. Nothing is atomic and
nothing publishes `g_toc` with a release.

Failure, the loud half: two threads reading the table range at startup both pass
`if (g_tocBuilt || g_tocFailed) return;` before either sets `g_tocFailed`. Both then run
`g_toc.resize(0x4000000)`, so one thread's ReadFile is still filling a buffer the other has freed and
reallocated, and both XOR the same 64 MB twice and insert the overrides twice. That is heap corruption
or an access violation on a loader path.

Failure, the quiet and more likely half: the second thread sees `g_tocFailed` already true, returns,
and `PatchTableRead` bails on `!g_tocBuilt`, so that chunk of the table is served straight from disk.
Those slots keep the package's own offsets, the overrides in them silently do not apply, and the log
still prints "table rebuilt".

Found independently by three reviewers.

In 68 captured runs the table was built exactly once each, read in one pass of 4 KB pieces through
the C runtime, so neither half has been seen. Nothing but that timing prevented it.

Also found on the same reading and fixed with it. The build wrote the three package sizes again,
unlocked, while `Hook_ReadFile` read them on other threads with no lock, and it now uses the ones
`TrackHandle` took and checks its own file is that size. And it ignored a failed seek, after which
the reads would have started at the front of the package and made its first 64 MB the table.

### F-02: the tracked handle list is read with no lock while it is written under one, on every ReadFile in the process
- severity: bug
- found-by: review
- batch: 1
- status: fixed
- fix: 26e69a4, 2026-09-20, by the proxy and core angle before this branch began. The hooks test `g_pkgHandleCount`, an atomic, and `IsPackageHandle` holds the lock for the scan.

`src/overlay/overlay.cpp:529`. `Hook_ReadFile` and `Hook_CloseHandle` read `g_pkgHandles` without the
critical section that `TrackHandle` takes to push into it.

Failure: the loader thread opens content.kspkg and `TrackHandle`'s `push_back` reallocates. At that
moment another game thread inside `Hook_ReadFile` evaluates `g_pkgHandles.empty()` against a half
updated header, reads "empty", and its chunk of the table read is served raw. Same silent outcome as
F-01's quiet half.

### F-03: an asynchronous table read is never patched and never noticed
- severity: bug
- found-by: review
- batch: 1
- status: fixed
- fix: f7366ed, 2026-09-23, the unnoticed half. A table read that goes pending is said once in the log, and the hook puts the read's error code back before it returns. Widened by H-02 in fb4c1f2 to every table read with an OVERLAPPED, narrowed by V-01 in 4c558ea to a read on a handle opened overlapped, and widened again by V-03 in 0c39e60 to take in a read whose OVERLAPPED carries an event.

`src/overlay/overlay.cpp:566` runs the patch only under `if (ok && got && ...)`. An overlapped ReadFile
that goes pending returns FALSE with ERROR_IO_PENDING. The trace line at 562 already has a branch that
prints "(async pending)", so async reads are known to happen in this process.

Failure: a game update switches the table read to an overlapped handle. The engine gets the real table
bytes from disk, the trackside screen fix, the UI stylesheet fix and every player override stop
applying, and nothing anywhere says so.

The unedited half stays. A pending read's bytes land after the hook returns, and editing them
safely would mean hooking the completion side too, the event, the completion port and the APC.
0.9.1 reads the table synchronously through the C runtime, so the line is for an update that
changes that. The error code fix was needed by this one, since the new log line sits on the very
path where a caller reads `ERROR_IO_PENDING`, and it also mends the trace line that already sat
there.

### H-01: a player's own file for either asset the mod corrects was silently replaced by the mod's copy
- severity: bug
- found-by: hunter
- batch: 1
- status: fixed
- fix: 1c8ab05, 2026-09-23, the player's file wins and the correction is skipped with a log line.

`CollectFiles` puts the player's files into `g_files` first, `AddBigScreenFix` and `AddUiStyleFix`
append theirs after, and the loop in `BuildToc` lets the last writer of a slot win. So with
`responsive_ui` on, the default, a UI mod's `acevo_mods\uiresources\css\uicomponents.css` never
reached the game and the log showed the entry replaced twice. The big screen texture behaved the
same with `fix_big_screens` on. Released in 0.3.2, so it has a changelog line.

### H-02: an overlapped table read that completes at once was edited after the game had been signalled
- severity: debt
- found-by: hunter
- batch: 1
- status: fixed
- fix: fb4c1f2 then 4c558ea and 0c39e60, 2026-09-23, a table read on a handle opened with `FILE_FLAG_OVERLAPPED`, or with an event in its OVERLAPPED, is left unedited and said once.

F-03's fix only noticed a read that went pending. One that completes inside `NtReadFile` has
already set its event or queued its completion packet before `ReadFile` returns to the hook, and
the first table piece then sits in `call_once` for the whole build, 80 to 160 ms. A worker thread
could take the raw bytes in that window, or hand the buffer to its next read so the edit landed in
someone else's data. Where the caller passed no byte count on an overlapped handle, the hook's own
substitute was filled from the OVERLAPPED after completion, which Microsoft warns against, and was
never clamped to the buffer. Leaving every overlapped table read alone removes both. Same future
update as F-03, since 0.9.1 passes no OVERLAPPED for the table.

### H-03: the layer's doc said the engine reopens the package without reading it
- severity: debt
- found-by: hunter
- batch: 1
- status: fixed
- fix: 2d685e6, 2026-09-23.

`logs/lap6-clean-retest-20260905-1948` shows 132 opens and 4 KB reads at 131 distinct offsets
outside the table through the C runtime. So the virtual read branch in `Hook_ReadFile` is a live
path for any overridden entry the engine reads that way, not one kept for completeness, which
matters when F-04 and F-06 are weighed in batch 2.

### H-04: the note F-03 added understated the damage when an override adds a file
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: fb4c1f2, 2026-09-23, the line says a package entry can go missing or appear twice.

If the table is read both ways, edited pieces and raw pieces disagree about where every slot past
an inserted one sits, so at each seam one stock entry is missing or doubled and the engine's lookup
of the missing one fails. The line only said the overrides in that part do not apply. And when
every table read goes pending `BuildToc` never runs, so "part of the table" was wrong as well.

### H-05: the file trace's line counter was a plain int bumped by any thread
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: 33e2241, 2026-09-23, an atomic, taken only when the line would print.

With `trace_file_io=1`, two threads reading the package at once could both pass the 200 line cap
or lose a count. A developer switch and only the line count, but the batch's own theme.

### H-06: each override's inserted flag was set and never read
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: 33e2241, 2026-09-23, removed. The count of added entries in `BuildToc` is a separate local and stays.

### V-01: leaving every table read that carried an OVERLAPPED unedited also caught a plain synchronous read
- severity: debt
- found-by: verifier
- batch: 1
- status: fixed
- fix: 4c558ea then 0c39e60, 2026-09-23, the layer remembers at open time whether a package handle was opened with `FILE_FLAG_OVERLAPPED`, and that or an event in the read's OVERLAPPED decides, never the OVERLAPPED alone.

Caused by H-02's fix in fb4c1f2. A read on a handle opened without `FILE_FLAG_OVERLAPPED` can pass
an OVERLAPPED only to give its offset. It is complete when `ReadFile` returns, and it was edited
before fb4c1f2. After it, an update that read the table that way would have lost both corrections
and every override, with the log blaming an asynchronous read that never happened.

No effect on 0.9.1, whose C runtime never passes an OVERLAPPED. The verifier read the three
`ReadFile` calls in the UCRT source of SDK 10.0.26100 and the four in the system `ucrtbase.dll`, and
each passes a null one. All 5580 package opens across the 68 runs came from `ucrtbase.dll`.

### V-02: the doc promised a log line whenever a later version stops reading the table synchronously
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 4c558ea, 2026-09-23, the Limits list says which moves are noticed and which are not.

The line fires only for a `ReadFile` the hooks see, on an overlapped handle or with an event. A
table read moved entirely to DirectStorage, a mapped view (the exe already imports `MapViewOfFile`
and `CreateFileMappingA`) or `ReadFileEx` never reaches `Hook_ReadFile`, so neither `table rebuilt`
nor the note prints and every override stops.

The verifier's third finding is in `src/ui/restyle_fix.cpp`, which belongs to
`sweep/review-ui-fixes`, so it went into that ledger as F-13. H-01 made it reachable for the main
stylesheet. A player's own `uicomponents.css` now reaches the game, and the restyle stub skips the
sibling walk on hover and focus changes on the strength of the stock stylesheets having no rule
that needs it, which a UI mod's file may not honour.

A second verifier pass ran on 4c558ea and the ledger, since that commit changed code. It confirmed
V-01 and V-02 and raised the six below, plus V-09 for batch 2.

### V-03: the fix for V-01 edited a read on an ordinary handle after its event had been set
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 0c39e60, 2026-09-23, only a read nobody but its caller hears about is edited, meaning a handle opened without `FILE_FLAG_OVERLAPPED` and no event in its OVERLAPPED.

Caused by 4c558ea. On a handle opened without the flag, a read whose OVERLAPPED carries an event
has the event set inside `ReadFile`, which the verifier confirmed on this machine. A thread waiting
on it could take the raw piece, or reuse the buffer, while the first piece sat in `call_once` for
the whole build. That is H-02's failure through another door. No 0.9.1 path reaches it.

### V-04: the doc's list of reads the hooks never see missed a package opened from a module loaded later
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 0c39e60, 2026-09-23.

`PatchEverywhere` patches the static imports of the modules loaded when `Install` runs. A later
version that opens the package from a DLL it loads afterwards, or through `GetProcAddress`, gets an
untracked handle whose table reads pass through unedited, overlapped or not, with neither `table
rebuilt` nor the note in the log when the whole table is read through it. DirectStorage's own core
loads that way today, which is harmless only because its reads go through the redirect.

### V-05: F-03's and H-02's fix lines described the rule 4c558ea replaced
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-23, both lines carry the whole history, the way the closed ledgers write a reworked fix.

### V-06: the reviews index still said every open angle's batches were waiting on the owner
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-23.

### V-07: F-13 in the ui fixes ledger is a verifier's finding under the review's prefix
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-23, the spec says how a finding handed over from another angle is numbered.

The telemetry ledger's F-11 and F-12 set the pattern on 2026-09-20 and the spec never said so. A
handed over finding joins the receiving ledger's own list and waits for a batch like the review's
findings, so it takes the next `F-` id, and its found-by names who raised it.

### V-08: the run counts covered only the top level session folders
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-23, 68 runs and 5580 opens, all from `ucrtbase.dll` with one table build each, so the conclusion held.

A third pass ran on 0c39e60 and 719a42c. It confirmed both, disassembled the compiled test to check
the mask, and raised the two below, both about words trailing the final rule.

### V-10: V-01's and V-02's lines described 4c558ea's rule, and F-03's history called 0c39e60 a narrowing
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-23, each now names the handle and the event, and F-03's history says 0c39e60 widened what 4c558ea had narrowed.

V-05 rewrote F-03 and H-02 for this very reason and missed V-01 and V-02, and its own rewrite of
F-03 wrote 0c39e60 down as a narrowing when it widened what 4c558ea left. Batch 2's fix for V-09
and F-06 builds on the handle tracking, and a reader taking V-01's line as the rule would have
treated a read with an event as edited.

### V-11: the doc said a table read the hooks never see leaves neither table rebuilt nor the note
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-23.

True only when no read of the table is seen at all. A later version reading the first pieces
through the C runtime and the rest through a mapped view gets `table rebuilt`, while the overrides
in the mapped part quietly stop, so a reader told that line rules out an unseen read would look
elsewhere.

A fourth pass ran on e869a7e, paper only. It confirmed the final rule is stated the same way in
every line, and raised three wording slips of that commit's own, V-12 to V-14.

### V-12: the doc's new sentence said table rebuilt prints once some table read is seen
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-23, it prints only once some read of the table has been edited, which a seen read on an overlapped handle or with an event is not.

### V-13: V-10's own heading and body misdescribed what was wrong with F-03's line
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-23, F-03's history already ended on the final rule and only called 0c39e60 a narrowing, which V-10 now says.

### V-14: V-04's body kept the claim V-11 corrected, without the qualifier V-02 gained
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-23, it holds when the whole table is read through the late handle, and V-04 now says so.

A fifth pass ran on 8d27f4a and looked only for statements that are false. It found none, and
raised the three below.

### V-15: V-04's paragraph was left with one line far longer than the rest
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-23, rewrapped.

### V-16: the spec said batch then severity, while every ledger orders a batch by the pass that raised each finding
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-23, the spec now says what all four ledgers do, since each pass's blocks answer the fixes just before them.

### V-17: the message of 8d27f4a says V-11 moved, and in that commit's diff it did not
- severity: nit
- found-by: verifier
- batch: 1
- status: wontfix
- fix: a pushed commit message stays as it is. The new blocks first went in above V-11 and were put below it before the commit, so the diff shows no move, and the order in the file is right.

### F-04: when the loose file cannot be opened the request is passed through with the invented virtual offset
- severity: bug
- found-by: review
- batch: 2
- status: fixed
- fix: 9293fae, 2026-09-23, a loose file that cannot be opened when the table is built is left out so the game reads the package's own entry, and a DirectStorage open that fails later is remembered and said once.

`src/overlay/overlay.cpp:614` returns false on an `OpenFile` failure, and `QueueProxy::EnqueueRequest`
at `proxy.cpp:276` then enqueues the original request, whose `Source.File.Offset` is the virtual offset
the mod itself wrote into the table. The real offset is no longer there, so there is no fallback and
DirectStorage is asked to read far past the end of the package.

Failure: `acevo_uicomponents.css` is written into the game folder at startup and overridden on every
machine by default. Antivirus quarantines that freshly written file, or a player deletes the acevo_
files mid session, and every UI stylesheet fetch becomes a read past end of file, so the menus come up
unstyled. The failure is also not remembered, so lines 606 to 613 re-enter the global critical section,
call `OpenFile` and write a log line on every single request for that entry, while every ReadFile in
the process waits on that same critical section inside `IsPackageHandle`.

What stays is the read after a later failure, which still fails. The slot already points past the
end of the package, and a file queue has no way to be handed bytes the layer no longer has, so the
fix makes it one line instead of a line per request. Left out at startup is the better outcome, the
game's own asset, and the check runs before the mod's own corrections so a player's unreadable file
does not also cost the correction for that entry. A player can meet the startup case with a file
another program holds open, so it has a changelog line.

### F-05: unlocked double checked read of o->dsFile, a plain pointer written inside the critical section by another thread
- severity: debt
- found-by: review
- batch: 2
- status: fixed
- fix: 2342bdf, 2026-09-23, the check and the read of the handle are inside the lock that writes it.

`src/overlay/overlay.cpp:606`. `OverlayRedirect` is called from `QueueProxy::EnqueueRequest`, which the
game drives from several streaming threads at once. Thread A holds the lock and lets `OpenFile` write
`o->dsFile`, thread B reads `if (!o->dsFile)` with no lock and no atomic. It survives on x86 store
ordering rather than by construction, and the pointer goes straight to `real->EnqueueRequest`.

### F-06: a virtual read on an overlapped handle signals only hEvent, so a caller using a completion port waits forever
- severity: debt
- found-by: review
- batch: 2
- status: fixed
- fix: 83aa09e, 2026-09-23, on a handle opened overlapped a virtual read fails at once with `ERROR_HANDLE_EOF`, which queues nothing, and is said once.

`src/overlay/overlay.cpp:543` fills the OVERLAPPED itself and returns TRUE without going near the
kernel, so no completion packet is ever queued. A reader that opened the package with
FILE_FLAG_OVERLAPPED and bound it to an I/O completion port passes a null hEvent, the `if (ev)` test
fails, nothing is signalled, and `GetQueuedCompletionStatus` never returns. `GetOverlappedResult` with
a null hEvent waits on the file handle, which is also never signalled. The doc records that this path
has never been exercised, which is why it would surface first on somebody else's machine.

Serving the read properly would need the port and key the handle is bound to, which means hooking
`CreateIoCompletionPort` as well, for a path 0.9.1 never takes. A read that fails at once is the
Win32 contract that leaves nobody waiting, and it is what the package itself answers at that offset.
The cost is that a later version reading a replaced entry this way gets a failed read rather than
the replacement, and the log says so.

### F-07: the ReadFile hook goes live one statement before the original it calls is resolved
- severity: debt
- found-by: review
- batch: 2
- status: fixed
- fix: fcf7ec2, 2026-09-23, ReadFile is hooked last, after every original its paths call is resolved, and not at all if one cannot be.

`src/overlay/overlay.cpp:589` returns with every module's import slot pointing at `Hook_ReadFile`, and
only line 590 then resolves `g_origSetFilePointerEx`. A non overlapped ReadFile on a tracked handle in
that window takes the else branch at line 532 and calls a null pointer. Only the fact that `Install`
runs inside DllMain, before the game's own threads exist, keeps the window shut.

### V-09: the virtual read branch decides by the OVERLAPPED rather than by the handle, so it can leave the file position behind
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 83aa09e, 2026-09-23, with F-06, the branch decides by the handle and moves the position on every read on a synchronous one.

The virtual branch of `Hook_ReadFile` moves the file position only when `ov` is null. On a handle
opened without `FILE_FLAG_OVERLAPPED`, a real read with an OVERLAPPED for its offset moves the
position too, which the verifier confirmed on this machine, 100 bytes read at offset 1000 leaving it
at 1100. So a caller that reads an overridden entry that way and follows with a plain read gets the
wrong bytes, while the doc's step 3 says the position advances.

Raised on batch 1's second verifier pass. It belongs with F-06, since both are the virtual branch
reading an OVERLAPPED as if it said what kind of handle it is on, and batch 1 now tracks the
handle's kind, so the fix has what it needs.

### F-08: the comment claims the older 32 MB table layout is handled and the branch below it gives up
- severity: debt
- found-by: review
- batch: 3
- status: open
- fix:

`src/overlay/overlay.cpp:413`. The comment `// older layout: 32 MB table` sits directly above a log
line saying the 64 MB table was not recognised and a return. `g_tocSize = 0x4000000` is hardcoded at
lines 389 and 106. `tools/kspkg.py` handles both sizes with `TOC_SIZES = (0x4000000, 0x2000000)`, so
the repo already knows the other layout exists. A reader chasing an overlay that quietly does nothing
reads that comment as a handled path.

### F-09: the package slot layout the whole file depends on exists only as bare hex
- severity: debt
- found-by: review
- batch: 3
- status: open
- fix:

`src/overlay/overlay.cpp:213` and about fifteen other places read slot fields as raw numbers: 0xE8 for
the hash at 213, 323, 417 and 441, 0xE4 for flags at 224, 334, 444 and 446, 0xE6 for the path length at
440, 0xF0 and 0xF8 for size and offset at 225, 226, 335, 336, 448 and 449, the 0x100 cipher bit at 235,
345 and 445, and 0xE0 as a path length limit at 428. The `hashAt` lambda is pasted verbatim three
times. `tools/kspkg.py`'s docstring lays the same slot out field by field, so the knowledge is in the
repo, just not in the file that writes into the structure.

### F-10: the package range check can wrap before it rejects
- severity: nit
- found-by: review
- batch: 3
- status: open
- fix:

`src/overlay/overlay.cpp:227`. `offset + size > g_pkgSize` overflows when offset is within 16 MB of
2^64. Not reachable today, because both call sites clamp size first and the negative LONGLONG then
fails `SetFilePointerEx`. `offset > g_pkgSize - size` is the ordering that cannot wrap.

### F-11: five hook counts named a, b, c, d and f
- severity: nit
- found-by: review
- batch: 3
- status: open
- fix:

`src/overlay/overlay.cpp:586` to 591, skipping e. The rest of the file names things properly.

## The runs

Batch 1, two launches on 2026-09-23 with the ini at its defaults.

`logs/overlay-b1-20260923`, one track loaded. The table was read once and built once in 95 ms, both
of the mod's own corrections applied with `2 replaced`, and all 134 package opens came from
`ucrtbase.dll`. Eight redirects served the flipbook twice and the stylesheet six times. No note of
any kind appeared, so no uneditable table read, no size mismatch and no failed seek, and the game's
own log has no `Exception Detected`. The race F-01 closes and the reads F-03 and H-02 are about never
happen on 0.9.1, so this run shows the rewritten paths broke nothing rather than the fixes working.

`logs/overlay-b1-mods-20260923`, the main menu only, with a copy of the mod's own narrowed stylesheet
placed at `acevo_mods\uiresources\css\uicomponents.css` as a player's file. It was moved out of the
game folder into that session afterwards. The log shows `1 loose file(s)`, the skip line for the
stylesheet fix, and `replace uiresources\css\uicomponents.css` once, where before H-01's fix it came
twice and the mod's copy took the slot. The stylesheet was redirected three times, to the player's
file, the only override left for that entry. No note and no exception.

## Checked and clean

The table parsing bounds are genuinely tight. The binary search bound, the `memmove` insert, the slot
scan and the varint walk in `BuildToc`, `AddBigScreenFix`, `AddUiStyleFix` and `FindVarintField` all
stay inside the 64 MB buffer, and `PatchTableRead`'s clamp is provably in range for both the source and
the destination. BUG-023, the 64 MB table kept for the whole session, is already tracked and is not
re-filed here.
