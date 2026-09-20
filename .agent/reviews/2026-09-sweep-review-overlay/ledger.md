---
name: review-2026-09-sweep-review-overlay
kind: review
description: the package override layer angle of the full review of main, an unlocked lazy build of a 64 MB table reached from a hook in every module, eleven findings, one breaks
updated: 2026-09-20
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

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the table is built once and published safely | pending | |
| 2 | a redirect that cannot be served fails visibly | pending | |
| 3 | the slot layout and the leftovers | pending | |

## Findings

### F-01: BuildToc is an unlocked lazy initialiser of a 64 MB shared vector, reached from a ReadFile hook installed in every module
- severity: breaks
- found-by: review
- batch: 1
- status: open
- fix:

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

### F-02: the tracked handle list is read with no lock while it is written under one, on every ReadFile in the process
- severity: bug
- found-by: review
- batch: 1
- status: open
- fix:

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
- status: open
- fix:

`src/overlay/overlay.cpp:566` runs the patch only under `if (ok && got && ...)`. An overlapped ReadFile
that goes pending returns FALSE with ERROR_IO_PENDING. The trace line at 562 already has a branch that
prints "(async pending)", so async reads are known to happen in this process.

Failure: a game update switches the table read to an overlapped handle. The engine gets the real table
bytes from disk, the trackside screen fix, the UI stylesheet fix and every player override stop
applying, and nothing anywhere says so.

### F-04: when the loose file cannot be opened the request is passed through with the invented virtual offset
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

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

### F-05: unlocked double checked read of o->dsFile, a plain pointer written inside the critical section by another thread
- severity: debt
- found-by: review
- batch: 2
- status: open
- fix:

`src/overlay/overlay.cpp:606`. `OverlayRedirect` is called from `QueueProxy::EnqueueRequest`, which the
game drives from several streaming threads at once. Thread A holds the lock and lets `OpenFile` write
`o->dsFile`, thread B reads `if (!o->dsFile)` with no lock and no atomic. It survives on x86 store
ordering rather than by construction, and the pointer goes straight to `real->EnqueueRequest`.

### F-06: a virtual read on an overlapped handle signals only hEvent, so a caller using a completion port waits forever
- severity: debt
- found-by: review
- batch: 2
- status: open
- fix:

`src/overlay/overlay.cpp:543` fills the OVERLAPPED itself and returns TRUE without going near the
kernel, so no completion packet is ever queued. A reader that opened the package with
FILE_FLAG_OVERLAPPED and bound it to an I/O completion port passes a null hEvent, the `if (ev)` test
fails, nothing is signalled, and `GetQueuedCompletionStatus` never returns. `GetOverlappedResult` with
a null hEvent waits on the file handle, which is also never signalled. The doc records that this path
has never been exercised, which is why it would surface first on somebody else's machine.

### F-07: the ReadFile hook goes live one statement before the original it calls is resolved
- severity: debt
- found-by: review
- batch: 2
- status: open
- fix:

`src/overlay/overlay.cpp:589` returns with every module's import slot pointing at `Hook_ReadFile`, and
only line 590 then resolves `g_origSetFilePointerEx`. A non overlapped ReadFile on a tracked handle in
that window takes the else branch at line 532 and calls a null pointer. Only the fact that `Install`
runs inside DllMain, before the game's own threads exist, keeps the window shut.

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

## Checked and clean

The table parsing bounds are genuinely tight. The binary search bound, the `memmove` insert, the slot
scan and the varint walk in `BuildToc`, `AddBigScreenFix`, `AddUiStyleFix` and `FindVarintField` all
stay inside the 64 MB buffer, and `PatchTableRead`'s clamp is provably in range for both the source and
the destination. BUG-023, the 64 MB table kept for the whole session, is already tracked and is not
re-filed here.
