---
name: review-2026-09-sweep-review-proxy-core
kind: review
description: the proxy and core angle of the full review of main, an off switch that takes the override layer with it and a row of ini values used without validation, fifteen findings plus nine from batch 1's hunter and verifier, one breaks
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

Fifteen findings, one breaks, six bug, four debt, four nit. Batches 1 to 3 added nine, nine and six
more, seven of those bugs the batches' own fixes caused or left standing.

Batch 3 is the one with no runtime gate. Both of its findings need a race or a reference count
fault the game has never produced, and 113 captured runs create exactly one factory each, so there
is nothing a launch can show. It is closed on reading rather than on evidence, which is worth
knowing when reading it back.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | an off switch turns off only what it names | closed, runtime confirmed | 2026-09-20 |
| 2 | every value that crosses the ini boundary is validated | closed, runtime confirmed | 2026-09-20 |
| 3 | one time init happens once, and a freed object is not left addressable | closed, no run can show it | 2026-09-20 |
| 4 | the log does not carry the player's machine into a public post | pending | |
| 5 | the leftovers | pending | |

## Findings

### F-01: turning stats off silently switches the whole package override layer off with it
- severity: breaks
- found-by: review
- batch: 1
- status: fixed
- fix: 530a0da then aa5a0c5, 2026-09-20, `QueueProxyWanted` names all the consumers that need the wrapper, the overlay among them, and each log line inside the wrapper checks its own setting.

The symptom below is understated and the verifier corrected it on 2026-09-20. The table rebase
runs through `Hook_ReadFile` and never touched the wrapper, so with `stats=0` the entries were
still rewritten to point at virtual offsets past the end of the package while the redirect that
serves them was dead. `logs/render-b4-20260920/acevo_perf.log:32` has the flipbook and the
stylesheet placed at 69,070,749,696 and beyond against a `content.kspkg` of exactly that many
bytes. So those two reads ran off the end of the file: the assets were unreadable, not merely
unfixed. `breaks` was the right severity for the wrong reason.

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
- status: fixed
- fix: 8c3ccb1 then 683f2fd then eb7c6d7, 2026-09-20, the value has to be one plain folder name with no slash, colon or leading dot, and `CollectFiles` stops at sixteen deep. Three attempts, because listing the bad inputs missed `..`, then `.`, then `.\`.

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
- status: fixed
- fix: 8c3ccb1 then 683f2fd, 2026-09-20, an explicit size is 128 to 1024 or the documented 0, and anything else falls back to auto rather than to the nearest bound.

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
- status: fixed
- fix: 8c3ccb1 then 683f2fd, 2026-09-20, lowercased, empty treated as auto, and a value that is neither a number nor auto says so instead of becoming zero. The same compare in `flags.cpp` and `adapter.cpp` went with it.

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
- status: fixed
- fix: 8c3ccb1, 2026-09-20, `IniIntInRange` reads both, 1 to 3600 and 100 to 1000000, and writes a line saying what it corrected.

`src/core/config.cpp:55` and `:117`. `IniInt` returns `_wtoi` unfiltered.

Failure with `stats_interval_s=0`, which a player could reasonably read as off: the test at
`proxy.cpp:204` becomes `dt < 0`, never true, so `Report` writes its roughly 400 byte line on every
`QueueProxy::Submit`. During a Nurburgring load that is thousands of blocking `WriteFile` calls a
second on the game's submit threads, through the same critical section the render thread uses. A
negative value converts to a huge unsigned and turns stats off in silence instead.

Failure with `sample_us=0`: `load_sampler.cpp:354` computes an interval of 0, the wait loop at 365
exits immediately, line 373 resets the deadline to now, and a `THREAD_PRIORITY_HIGHEST` thread
suspends and resumes the game's busiest threads with no delay at all. The only way out is killing the
process. The telemetry doc does not cover it, which is the agent directory angle's F-10.

That last sentence originally read "in neither the shipped ini nor the telemetry doc". The ini half
is wrong: `dist/acevo_perf.ini:63` has shipped `sample_us=1000` with a note all along, and the
commit that fixed this finding edited that very line. Corrected on 2026-09-20 by batch 2's hunter.

The codebase already knows these need bounds. `minQueueCapacity` is clamped to the DirectStorage
minimum and maximum at `proxy.cpp:337` to 340, at the use site rather than at the read.

Found independently by three reviewers.

### F-06: the one time late init block sits outside the lock the same function takes eight lines later
- severity: bug
- found-by: review
- batch: 3
- status: fixed
- fix: a1e82ec then 26e69a4, 2026-09-20, both pieces of one time work run under `g_realCs`, which is what the title asked for. An atomic exchange was tried first and gave exclusion without ordering.

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
- status: fixed
- fix: a1e82ec then 26e69a4, 2026-09-20, the global is cleared under the lock before the object goes, and the factory is handed out as a counted reference rather than a bare pointer so the caller cannot be left holding a freed one.

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

### H-01: the stats gate silenced a line that carries its own switch
- severity: bug
- found-by: hunter
- batch: 1
- status: fixed
- fix: aa5a0c5, 2026-09-20, the gate sits on the `[stats]` line, not at the top of the function.

F-01's first fix put `if (!g_cfg.stats) return;` at the top of `QueueProxy::Report` so a player
who asked for quiet still got it. Eleven lines below sat the repeated read total, guarded by
`streamingTrace`, which the early return made unreachable. So `streaming_trace=1` with `stats=0`
lost a line it used to get, and the telemetry doc promises it without mentioning `stats`.

### H-02: the timeline CSV, the frames CSV and the hitch lines were still hostage to stats=0
- severity: bug
- found-by: hunter
- batch: 1
- status: fixed
- fix: aa5a0c5, 2026-09-20, `QueueProxyWanted` lists every consumer by name.

The wrapper is the only writer of `g_reqByDest`, `g_bytesByDest`, `g_submitsTotal`, `g_tileBatches`
and `g_tileBatchMax`. The timeline CSV reads nine columns from them, the `[hitch]` line reads three,
and neither `timeline` nor `frame_stats` was in the wrap condition. With `stats=0`, `enabled=0` and
`timeline=1` those columns are all zero for the session with nothing saying why. That is this
batch's own theme reached from a different direction, and it is the shape the render angle already
fixed once for `frame_stats`.

### H-03: the box a player sees on a broken install told them to copy three files out of five
- severity: bug
- found-by: hunter
- batch: 1
- status: fixed
- fix: aa5a0c5, 2026-09-20, it says to copy all the files, which is what the readme and the zip readme say.

The message named `dstorage.dll`, `dstorage_orig.dll` and `acevo_perf.ini`. `release.ps1` ships
five, and the one left out is `acevo_dstoragecore.dll`, the DirectStorage 1.3.0 the mod exists to
bring. A player who followed it got a game that loads, falls back to the game's own 1.2.3 with one
log line, and stays that way. It also broke the public docs rule that install text never lists file
names.

### H-04: the factory handed its real self to anything that was not exactly IDStorageFactory
- severity: debt
- found-by: hunter
- batch: 1
- status: fixed
- fix: aa5a0c5, 2026-09-20, `IUnknown` is answered with the proxy.

`QueueProxy::QueryInterface` declines `IDStorageQueue3` on purpose, with a comment saying a newer
runtime must not let the game slip past the proxy. One level up the same hole was open: any riid
that was not `IDStorageFactory`, `IID_IUnknown` included, got the unwrapped factory, and every
queue made from it would escape the staging override, the capacity raise and the redirect. Latent,
the line has never appeared in a log.

### H-05: the comment justifying the early return said something the code contradicts
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: aa5a0c5, 2026-09-20, the comment went with the code it justified.

"The final line is the only thing that reads them" was false: `Submit` reads two of the counters on
every call to decide whether this is the tile queue, and both feed the timeline CSV.

### H-06: a doc gained a dated paragraph and kept its old date
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: aa5a0c5, 2026-09-20.

`package-override-layer.md` said `updated: 2026-09-15` while the paragraph added to it said "until
2026-09-20".

### V-01: the ledger was two commits behind its own spec
- severity: bug
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-20, in the commit that closed the batch.

F-01 still read `status: open` with an empty `fix:` after being fixed twice, none of the six hunter
findings had a block at all, the batch row said pending and the summary's count was stale. The
spec's whole point is that the ledger is the record, and a record two commits behind the code is
the thing the upkeep rule exists to prevent. The render angle got this right by updating the ledger
once per batch, which is what this now does.

### V-02: one term of the new condition is not needed
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-20, the term is gone and the comment says why.

`g_cfg.frames` was listed as a consumer. The frames CSV's rows come from a buffer filled only
inside `OnPresent`, which returns early when `frame_stats` is off, so `frames` can never produce a
row without `frame_stats` being on as well. Harmless, it only over wrapped, but a list whose point
is to name the consumer that needs each term is worse for carrying one that does not.

### V-03: the error line has no switch of its own and goes with the wrapper
- severity: debt
- found-by: verifier
- batch: 1
- status: wontfix
- fix:

`RetrieveErrorRecord` is where the mod surfaces a DirectStorage request failure, and it lives in
the wrapper like everything else. With all seven consumers off it is gone, and that is the one
configuration where a failure would be least visible. It matters because batch 2's F-03, a staging
size of 4096 wrapping to zero and every request failing, would be silent there.

Not fixed, because the alternative is wrapping every queue always, and the configuration that
leaves it unwrapped is the project's own passive control for frame time measurements. Adding eight
atomics a request to that would corrupt the measurement it exists for. Recorded so the next reader
knows the gap is deliberate.

## The run

Session `logs/proxycore-b1-20260920`, the owner's launch to a loaded track on 2026-09-20 with
`[directstorage] stats=0` set for that one run and put back afterwards. The first session on disk
ever to use that setting, and the one that used to break the override layer.

- Eight `overlay: redirected request` lines, the flipbook for the trackside screens and the UI
  stylesheet, both served from the mod's own files. Before the fix none of them would have run and
  both reads would have gone past the end of `content.kspkg`.
- Not one `[stats] queue` line in the whole run, so a player who asked for quiet still gets it.
  That is the half the first fix got wrong once already.
- All three queues created and wrapped, including `GpuUpload Memory Queue`, which the overlay can
  never use but which feeds the counters the CSVs read.

There is no H-07. The block that held it was the verifier's, not the hunter's, and became V-04 when
the ids were made unique on 2026-09-20. Ids are handles, so the gap stays rather than six blocks
shifting under any commit that already names them.

### V-04: a yes or no value the list did not know came back as no, for thirty one keys
- severity: bug
- found-by: verifier
- batch: 2
- status: fixed
- fix: eb7c6d7, 2026-09-20, `IniBool` knows both spellings of both answers and a word outside them keeps the shipped value and says so.

The batch's whole theme, in the reader that handles most of the file. Any word `IniBool` did not
recognise returned false, so `reflex=ture` turned Reflex off, `[dxgi] enabled=ture` took the frame
times and Reflex with it, and `[overlay] enabled=ture` killed the override layer, all in silence.
A word we do not know is a typo and not a no.

Found because the changelog line this batch wrote claimed settings typed wrong no longer break the
game quietly, which was true for about six keys out of forty.

### H-08: the staging floor was set below the size that works, and its own ini hint advertised it
- severity: bug
- found-by: hunter
- batch: 2
- status: fixed
- fix: 683f2fd, 2026-09-20, the floor is 128, and the comment that put the game's largest request at 32 MB is corrected.

A request larger than the staging buffer fails outright. The first clamp accepted 1 to 1024 and the
shipped ini was edited to say so, which advertises sizes where every large request fails. That is
F-03's symptom at the other end of its own range.

The floor rests on a number the tree had wrong. `AutoStagingMb`'s comment said the game's largest
request is 32 MB. Across all 7,848 `max req` values on disk the largest is 98,541 KB, 96.2 MB, and
32 MB is only the ninetieth percentile. So 128 MB holds the largest request with a little room,
rather than four of them as the comment claimed, and it is the floor for that reason.

### H-09: clamping to the nearest bound landed on the value three filed bugs name as the cause
- severity: bug
- found-by: hunter
- batch: 2
- status: fixed
- fix: 683f2fd, 2026-09-20, out of range falls back to auto.

4096 clamped to 1024. 1024 is what the game asks for by itself, it is what the mod exists to
override, and BUG-003, BUG-004 and BUG-005 all trace to it. So the correction answered a bad value
with a known bad one and wrote a note saying it had fixed it. Auto already knows the card.

### H-10: the same unlowercased auto compare stood in two more files
- severity: bug
- found-by: hunter
- batch: 2
- status: fixed
- fix: 683f2fd, 2026-09-20, `SplitFlag` lowercases the value and `WantsAutoSizes` compares case insensitively.

F-04 was about one compare. There were three. `tile_pool_mb=Auto`, written against a shipped line
whose own note says "auto picks it for your card", missed the auto branch, reached `WriteFlag`, and
`atoi` turned it into a literal 0 written into the engine's tile pool size at both storage slots.
Zero there hands the canonical flag the whole `texturePoolSize` define, which is the video memory
exhaustion of BUG-003 to BUG-005.

### H-11: an empty log file name turned the log off for the whole run
- severity: bug
- found-by: hunter
- batch: 2
- status: fixed
- fix: eb7c6d7, 2026-09-20, empty falls back to the default name, and a path that cannot be opened falls back to the shipped one and says why in it.

`file=` present and empty made `DllMain` prepend the game folder to nothing and open a directory,
which fails, and every `Log` in the process then returned for the rest of the session. The player
gets no file at all and reads it as the mod not loading. Nothing could report it, because the report
goes through `Log`. The verifier then pointed out the empty value was only one way in, so the open
itself now has a fallback.

### H-12: a word other than auto still meant no staging cap, silently
- severity: bug
- found-by: hunter
- batch: 2
- status: fixed
- fix: 683f2fd, 2026-09-20.

`staging_buffer_mb=default`, `=off`, `=none` all reached `_wtoi`, came back 0, and left the cap off
with the log saying only `staging=0MB`, which is verbatim the failure F-04 describes.

### H-13: an unknown word in priority or gpu_priority landed on an active setting
- severity: debt
- found-by: hunter
- batch: 2
- status: fixed
- fix: 683f2fd, 2026-09-20, both say so and keep the default, and `tile_queue_priority` says so too.

Both ended in a bare else. `priority=low` gave above normal, raising the process when the player
asked to lower it, and any typo in `gpu_priority` set the scheduling class high. The resolved value
is the same as before in both cases, since the fall through already landed on the default. What was
missing was anything telling the player their word was not understood.

### V-05: the folder guard listed the bad inputs and missed one
- severity: bug
- found-by: verifier
- batch: 2
- status: fixed
- fix: eb7c6d7, 2026-09-20, the value must be one plain name, which is the rule rather than a list of exceptions to it.

`folder=.\` and `folder=./` passed every term of the guard: not empty, not `.`, not `..`, no `..`
inside, no colon, and not starting with a slash. They resolve to the game folder, which is F-02's
exact failure. Two rounds of listing what is wrong, and the third round states what is right.

### V-06: three statements this batch made about its own ranges
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 683f2fd for the two ini hints, eb7c6d7 for the rest, 2026-09-20.

The two remaining ini hints named a floor and no ceiling while the code enforced both. The staging
hint did not mention that 0 is still accepted. And the comment justified the 1024 ceiling by the
overflow, which happens at 4096, so 1025 to 4095 are refused for a reason the comment did not give.
The real reason is that this is video memory taken from rendering and 1024 is already the size the
mod exists to override.

### V-07: a second yes or no reader in another file still answered a typo with no
- severity: bug
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-20, in the commit that closed the batch. `WriteFlag` knows both lists and leaves the flag alone on anything else, saying so.

V-04 fixed the ini reader. The `[flags]` section does not go through it, it is walked raw and
written by `WriteFlag`, which had the same list and the same answer. Three bool flags ship in that
section and all three ship on, so `force_canonical_pool_sizes=ture` wrote false into the engine and
put back the blurry textures after a race load that the changelog claims are fixed. Not quite
silent, the log said `= false (bool, was true)`, but nothing said the word was not understood.

Two readers of the same kind of value in one program with opposite answers to a typo is the thing
worth remembering here, not either one of them.

### V-08: the ledger broke three rules of its own spec
- severity: bug
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-20.

Batch 2's blocks reused `V-01` and `V-02`, which batch 1 already held, so neither id named one
finding any more. One verifier finding carried an `H-` prefix. And six blocks used
`found-by: verifier`, which the spec's own enum did not list.

The spec was the thing that was wrong on the last of those, since the house rules have run a
verifier on every batch of this review, so it gained the value and a sentence saying ids are unique
across the file rather than per batch. The other two were the ledger's.

## The runs

Batch 1: session `logs/proxycore-b1-20260920`, `[directstorage] stats=0` for one launch. Eight
`overlay: redirected request` lines, the trackside screen flipbook and the UI stylesheet among
them, and not one `[stats] queue` line. Before the fix the redirect would not have run at all and
those two reads would have gone past the end of the package.

Batch 2: session `logs/proxycore-b2-20260920`, six values deliberately wrong in one launch. Each
one used to do damage quietly, and each wrote a line saying what happened instead.

- `staging_buffer_mb=4096` fell back to auto and `SetStagingBufferSize(128 MB)` landed. Before, the
  conversion gave zero bytes and every request that exceeded it failed.
- `[overlay] folder=.` became `acevo_mods`, and the log says that folder is not present. Before,
  the layer would have walked the whole game install from DllMain and offered every file in it.
- `reflex=ture` left Reflex on, confirmed by `[reflex] on` and by `reflex=1` in the config line.
  Before, an unrecognised word was a no and Reflex was off with nothing saying so.
- `stats_interval_s=0` became 1, so 61 stats lines rather than one per submit.
- `tile_queue_priority=fastest` left the game's own priority and said the word was not known.
- `force_canonical_pool_sizes=ture` printed `left alone` four times, twice per storage slot across
  the early and late passes.

One honest qualification on that last one. All three bool flags the mod ships in `[flags]` have an
engine default of false, so leaving the flag alone lands on the same value the old code wrote when
it read a typo as a no. For those three the outcome is identical and only the log changed. The
value differs only for an engine flag whose own default is true, which none of the shipped three
is. V-07 is worth having for the visibility and for the two readers no longer disagreeing, not for
a changed number on this ini.

The override layer still worked throughout, `overlay: redirected request #1` at 16:58:12.

### V-09: the lock guarded the read of the pointer and not the use of it
- severity: bug
- found-by: hunter
- batch: 3
- status: fixed
- fix: 26e69a4, 2026-09-20, `RealDStorageFactory` hands out a reference and the overlay keeps one for the run.

F-08's first fix took the lock, read `g_factory->real`, released the lock and returned the bare
pointer. The caller then used it with the lock gone, and the last `Release` can land in that
window, free the real factory, and have `OpenFile` called on it. F-08's own failure, one
instruction later, with the ledger about to say fixed and a comment asserting the hole was closed.

### V-10: clearing the global made a live no factory state in which the layer sends reads off the end of the package
- severity: bug
- found-by: hunter
- batch: 3
- status: fixed
- fix: 26e69a4, 2026-09-20, the overlay's own reference means the factory cannot go while the layer needs it.

The old state was a dangling global, the new one was a null global with the package table still
rebased and three queues still live. `OverlayRedirect` returns false on a null factory and
`EnqueueRequest` then passes the request through unchanged, still carrying a virtual offset that
sits past the end of `content.kspkg` by construction. That is F-01's confirmed failure, reached
from the fix for F-08.

### V-11: the atomic gave run once, not wait until done, and the finding's own title named the lock
- severity: bug
- found-by: hunter
- batch: 3
- status: fixed
- fix: 26e69a4, 2026-09-20, both pieces of one time work happen under `g_realCs`, for both branches of the export.

F-06's first fix made the two guards `std::atomic` and used `exchange(true)`. That stops the block
running twice and does nothing about ordering. The loser no longer ran the work and no longer
waited for it either, so it fell through to the factory creation while the winner was still inside
`DStorageSetConfiguration1`, and the runtime refuses the configuration once a factory exists. On
the fallback path it also read a staging size that had not been filled in yet.

F-06's title is "the one time late init block sits outside the lock the same function takes eight
lines later". The lock gives exclusion and ordering in one move. The atomic gave one of the two.

### V-12: the export lost the configuration on the one path that can also create a factory
- severity: bug
- found-by: verifier
- batch: 3
- status: fixed
- fix: 26e69a4, 2026-09-20, the unimplemented interface branch runs under the same lock and after the same one time work.

Caused while fixing V-11. Moving the one time work inside the lock left it below the early return
that forwards an interface the proxy does not implement, and that return calls the real
`DStorageGetFactory`, which creates a real factory. So a first call asking for anything unusual
would have built a factory with no configuration applied at all.

Also found on the same reading, and fixed with it: the old code returned the runtime's `S_OK` when
the call succeeded but handed back a null factory, leaving the caller's out parameter never
written. Nobody filed that.

### V-13: four statements about the new locking, three of them in this batch's own commits
- severity: nit
- found-by: verifier
- batch: 3
- status: fixed
- fix: 2026-09-20, in the commit that closed the batch.

The header still described `RealDStorageFactory` as handing back a pointer, with the ownership rule
only in the source, so the next caller would leak a reference per call. A comment said the function
runs once per override file when the same commit's cache made it once per process. The lock order
comment stated a rule the batch itself broke twice, since the overlay now reaches `g_realCs` from
its own lock and the runtime report reaches the loader lock from inside `g_realCs`. What keeps the
process alive is that the long hold and the waiter cannot overlap in time, because no thread can be
in a redirect until a factory has been handed out, and that is the thing worth writing down.

And `proxy-architecture.md` had a current date over five wrong statements: the order of step 2 was
backwards, the fallback's position relative to the factory had moved, `StartLoadSampler` was never
mentioned, the lock that serialises the whole step was absent from a doc whose job is load order,
and the timing figure was measured before the threads moved. Eighth instance on this review.

### V-14: the same one time flag one call out
- severity: nit
- found-by: verifier
- batch: 3
- status: fixed
- fix: 2026-09-20, an exchange, with the reason it is enough there written beside it.

`g_resolveDone` in `adapter.cpp` is the batch's own theme one file over, a read then a write with
two callers, and this batch put one of them under a lock the other does not take. An exchange is
enough for that one, unlike the export's, because the loser has no use for the result and only
needs the work not to happen twice. Saying which of the two shapes applies and why is the point.

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
