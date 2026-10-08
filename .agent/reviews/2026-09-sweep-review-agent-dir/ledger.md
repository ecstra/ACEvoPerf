---
name: review-2026-09-sweep-review-agent-dir
kind: review
description: the agent directory angle of the full review of main, knowledge docs describing code that changed underneath them and tracker files that break their own specs, twenty four findings and one added by another angle's verifier, 24 fixed and 1 wontfix, merged into 0.4 on 2026-10-08
updated: 2026-10-08
links: [spec-reviews, house-rules-agent, conventions, spec-bugs, spec-todos, reviews-index]
branch: sweep/review-agent-dir
status: closed
---

# Review of the agent directory

## Summary

Two angles of the full review of main run on 2026-09-20, filed together because they change the same
tree. One reviewer took every checkable claim in the foundation, systems and ops docs and tested it
against the source. A second audited all 143 files under `.agent/` against the specs in
`.agent/spec/`, mechanically, with every count and every link checked.

The mechanical state is good. Every index count is correct, every index line points at a file that
exists, every file has an index line, and there are zero dead links across all 143 files. No secrets, no
usernames, no machine paths, no em dashes.

What drifted is the prose. The architecture doc describes a source tree that has moved on, two shipped
subsystems are documented nowhere a reader would look, and a handful of tracker files break the section
shapes their specs fix. CLAUDE.md says a file that breaks its spec is a bug, so those are filed as such.

Twenty four findings, eight bug, nine debt, seven nit. F-25 was added on 2026-09-29 by the verifier of
`sweep/review-telemetry`, which met it while checking the deep dive's sampler numbers.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the docs stop describing code that is no longer there | closed | 2026-10-08 |
| 2 | the parts nothing documents get a home | closed | 2026-10-08 |
| 3 | the tracker files match their specs | closed | 2026-10-08 |
| 4 | the dates the 2026-09-18 round skipped | closed | 2026-10-08 |
| 5 | memory and handover boundaries | closed | 2026-10-08 |
| 6 | the leftovers | closed | 2026-10-08 |

On 2026-10-08 the owner asked for the remaining angles to be finished fast, with manual checks in place of
the hunter and verifier loop. The architecture and ops docs were fixed by hand against the code, and the
bug, todo, memory and deep dive files by three sub agents working on separate files in parallel, each
told to edit only its own files and commit nothing, then read through by hand before commit. Their
reports raised what they saw and left alone, and the clear ones were fixed in the same pass, three bugs
filed after the review with the same section gaps, two leftover sections in closed todos, a hyphen in an
index line, a stale byte identical claim and a mid sentence colon in `build-and-release.md`, and one more
personal detail in a research doc. Three earlier angles had already fixed F-18, F-19 and part of F-03 and
F-20, and later commits had overtaken F-08.

## Findings

### F-01: the architecture doc says the overlay only installs its file hooks when the mods folder holds files
- severity: bug
- found-by: review
- batch: 1
- status: fixed
- fix: 2026-10-08. Step 5 says the overlay installs whenever it is on, with no mods folder at all, for the two generated fixes, and points at `package-override-layer`.

`.agent/docs/foundation/proxy-architecture.md:54`. The overlay installs those hooks with no mods folder
at all, because the trackside screen fix and the UI stylesheet fix are generated from the player's own
package and need them. `src/overlay/overlay.cpp:584` sets
`g_active = g_cfg.overlayEnabled && (!g_files.empty() || g_cfg.fixBigScreens || g_cfg.responsiveUi)`,
both flags default true, and lines 578 to 581 log that the folder is absent and the asset fixes still
apply. `package-override-layer.md:47` already says this, so the foundation doc contradicts the system
doc.

### F-02: ApplyProcessTweaks is enumerated as three things and does five
- severity: bug
- found-by: review
- batch: 1
- status: fixed
- fix: 2026-10-08. Step 1 and the engine row name all five, the GPU scheduling priority through `D3DKMTSetProcessSchedulingPriorityClass` from `gpu_priority`, on at high, and the working set floor from `working_set_floor_mb`, off.

`.agent/docs/foundation/proxy-architecture.md:31`, repeated in the engine row at line 19. It also sets
the process D3DKMT GPU scheduling priority class and the working set floor,
`src/engine/process.cpp:106` and `:107` calling `ApplyGpuPriority` and `ApplyWorkingSetFloor`. GPU
priority ships on at high, so a shipped default that calls into gdi32's
`D3DKMTSetProcessSchedulingPriorityClass` is described in no doc, and neither is
`[process] gpu_priority` nor `working_set_floor_mb`.

### F-03: the source map table omits three files, two of them a shipped feature nothing documents
- severity: bug
- found-by: review
- batch: 2
- status: fixed
- fix: 2026-10-08. The render row and a Reflex and a texture_writes piece were already added by earlier angles. The core row now lists `code_patch` and the pieces give it an entry.

`.agent/docs/foundation/proxy-architecture.md:17` and `:20`. The core row lists log, config and iat, and
`src/core/code_patch.cpp` also exists, the foundation every in memory patch stands on, used by six
files. The render row lists dxgi_hooks, frame_stats and adapter, and `src/render/reflex.cpp` and
`src/render/texture_writes.cpp` also exist.

Reflex is the sharper half. It ships on by default with its own ini section, `reflex.cpp:125` called
from `frame_stats.cpp:116` and `OnFrameBegin` running on every present, and a grep of foundation,
systems and ops finds no mention of reflex, of `[latency]` or of texture_writes anywhere. The only
coverage is the research doc `reflex-2026-09-12`, which is not where a reader looks for what the DLL
contains.

Found by both reviewers independently.

### F-04: the streaming doc says streamer.cpp writes the census
- severity: bug
- found-by: review
- batch: 1
- status: fixed
- fix: 2026-10-08. The paragraph says the file feeds the developer trace its streamer rows, that its census is gone in 9518e7d, and where the memory census lives.

`.agent/docs/systems/directstorage-streaming.md:43`. The streamer census was deleted in commit 9518e7d.
`src/engine/streamer.cpp` has no census code and includes only `telemetry/streaming_trace.h`. The only
census left is `src/telemetry/memory_census.cpp`, a different subsystem behind a different flag.

### F-05: a fixed bug with no Verification section, wrong section names, and a date two days behind its own content
- severity: bug
- found-by: review
- batch: 3
- status: fixed
- fix: 2026-10-08. Problem, Evidence, Fix and Verification in order, the dated sections as subsections under Evidence and Verification with their text unchanged, the Done when heading gone with its closing paragraph kept, `reported: 2026-09-12` added, and the description says it was fixed in 1363062 and closed in 75894a6.

`.agent/bugs/BUG-016-vram-overhead-grows-across-scene-loads.md`. `spec/bugs.md` fixes the body sections
as Problem, Evidence, Fix, Verification. This file has none of those four headings, it runs Symptom,
Measured, Reading and four dated sections, and it ends on "Done when", which is a todos section. It is
status fixed, closed in 75894a6 on 2026-09-18, and carries no Verification heading at all while
`updated:` says 2026-09-16 and its own last two sections are dated 2026-09-18. Its `description:` still
says the fix is sitting on a branch. It is also the only one of the thirty bug files missing the
required `reported:` field, which its earliest evidence puts at 2026-09-12.

### F-06: a todo using frontmatter keys the spec does not define, with neither What nor Done when
- severity: bug
- found-by: review
- batch: 3
- status: fixed
- fix: 2026-10-08. `born:` and `by: owner` in the frontmatter, the done when text moved into a Done when section, and the old sections now subsections of What.

`.agent/todos/TODO-010-resume-the-one-percent-low-hunt.md:9`. `created:` where the spec says `born:`, a
free form `done-when:` in frontmatter that belongs in the body, no `by:` at all, and body sections that
are none of the two the spec requires.

### F-07: a second Verification heading states Absent on a bug that is verified directly above it
- severity: bug
- found-by: review
- batch: 3
- status: fixed
- fix: 2026-10-08. The stub heading and its "Absent" are gone, and the real Verification stands alone.

`.agent/bugs/BUG-007-blurry-road-and-textures.md:47`. Line 41 holds the real evidence, the owner after
lap four of 2026-09-05 with tile traffic going from 700 MB to 1.7 GB. Line 47 is a leftover template
stub whose whole body is the word "Absent". A false claim sitting inside a fixed bug.

### F-08: the 2026-09-18 commit round changed six agent files and bumped none of their dates
- severity: bug
- found-by: review
- batch: 4
- status: fixed
- fix: 2026-10-08. Overtaken by later work. All six files have since been edited with their dates moved, the two indexes, BUG-016 and `session-leak-fix.md` to 2026-09-24 and later, the spine and `telemetry.md` to 2026-10-08, so none still reads 2026-09-16.

`spec/conventions.md` says any change to a file updates its `updated:` date and its index line in the
same commit. Commits 0c7bfc6, 11f5d5c, 75894a6 and afd3e12, all on 2026-09-18, made real content edits
to six files that still read 2026-09-16: `.agent/INDEX.md` (the fixed count went from 13 to 14),
`.agent/bugs/INDEX.md` (BUG-032 added, BUG-016 moved), `.agent/todos/INDEX.md` (TODO-020's hook
rewritten), `.agent/bugs/BUG-016` (two new sections), `.agent/docs/ops/telemetry.md` (a new census
freeze paragraph) and `.agent/docs/systems/session-leak-fix.md` (a rewritten coverage paragraph).

### F-09: the streaming CSV kind list is missing the write rows
- severity: debt
- found-by: review
- batch: 2
- status: fixed
- fix: 2026-10-08. The kind list has `write` with its ten columns, read off `NoteWrite`, and a paragraph says what the `[writes]` line carries.

`.agent/docs/ops/telemetry.md:85` lists kick, tex, want, drop, drop0, req and reread. A `write` kind goes
into the same file from `NoteWrite` in `src/render/texture_writes.cpp` for every copy into a streamed
texture that did not come from DirectStorage, and the `[writes]` log line in `TextureWritesTick` is
undocumented too. A
reader parsing the CSV from this doc meets a row kind the doc denies exists.

### F-10: the load sampler has no section and its CSV is never named
- severity: debt
- found-by: review
- batch: 2
- status: fixed
- fix: 2026-10-08. `telemetry.md` has an `acevo_perf_load_samples.csv` section, its header, one row per second holding only that second, `t_s` from attach, and the `[loadsampler]` log summary.

`.agent/docs/ops/telemetry.md:14`. The doc's own description is the log and CSV files the mod writes and
their columns, and it gives a section to four of them. The load sampler appears only as a name in a
list, while it writes a fifth file, `acevo_perf_load_samples.csv`, plus a block of log lines, with no
columns documented anywhere. Since the telemetry branch's cb7fccf each row holds its own second, and
since its 64e55ad the rows' t_s counts from attach like every other file's, which is what the section
should say.

### F-11: an unqualified claim about the render thread that the developer path breaks
- severity: debt
- found-by: review
- batch: 1
- status: fixed
- fix: 2026-10-08. The bullet names the command list copy hooks `streaming_trace=1` installs as the exception.

`.agent/docs/foundation/proxy-architecture.md:102` says the mod never hooks anything on the render thread
beyond Present and the UI frame post and end. With `streaming_trace=1` it patches four
`ID3D12GraphicsCommandList` vtable slots on the direct, compute and copy list vtables
(the `PatchSlot` calls in `TextureWritesOnSwapChain`, which both swap chain hooks in
`dxgi_hooks.cpp` call). The third bullet at line 103
saves the default case, and the sentence as written is false.

### F-12: two open bugs carry no Verification section while the other ten do
- severity: debt
- found-by: review
- batch: 3
- status: fixed
- fix: 2026-10-08. Both end on Fix and Verification, "Absent.", and BUG-018 also gained an Evidence heading over its dated sections, with its Done when made a subsection of it. Three bugs filed after the review, BUG-035, BUG-036 and BUG-038, had the same gaps and the Symptom heading, and got the same shape.

`.agent/bugs/BUG-018-whole-scene-low-detail-for-a-second-after-load.md` has no Fix and no Verification
heading at all, and `.agent/bugs/BUG-021-textures-blur-after-camera-cuts-at-the-red-bull-ring.md` ends at
line 134 with none. Every other open bug ends on Verification with the body "Absent", which is the shape
the spec asks for.

### F-13: a fixed bug whose Fix section names no commit and no date
- severity: debt
- found-by: review
- batch: 3
- status: fixed
- fix: 2026-10-08. The Fix section names `9ffe4a3` of 2026-09-12, the first commit `git log -S fix_big_screens` finds. Left as found, its text still describes the loose file route of `tools/texture_mips.py`, while that commit changes a header byte inside the mod, which only its Verification says.

`.agent/bugs/BUG-017-trackside-big-screens-blurry.md:67`. The spec says the Fix section holds the root
cause, the commit hash and the date. This one describes the mip trim and names the tool, with no hash and
no date. Twelve of the fourteen fixed bugs give a hash.

### F-14: five closed todos name no commit in Done when
- severity: debt
- found-by: review
- batch: 3
- status: fixed
- fix: 2026-10-08. Each names its commits in Done when, found from `git log --follow` on the todo and the files the work touched, and every hash resolves.

`spec/todos.md` says a closed item names its commits there. TODO-002, TODO-004, TODO-007, TODO-015 and
TODO-027 close with prose only. Seven of the twelve done todos do name hashes, so the convention exists
and these five miss it.

### F-15: a handover edited the day after it was written, which its spec forbids
- severity: debt
- found-by: review
- batch: 5
- status: wontfix
- fix: 2026-10-08. Taking the block out now would be one more edit after the fact, and it would leave the handover pointing a reader at a race line its own author found confounded. The snapshot below the block is as it was written, and the rule is kept from here on.

`.agent/handover/2026-09-13-frame-time-mesh-budget.md:5`. `spec/handover.md` says a handover is a
snapshot, dated, and never updated after the fact, and that a resumed stream that pauses again writes a
new one and links the old. Commit 5584d3e on 2026-09-14 inserted a supersession block at line 11, added a
link and moved `updated:` forward. The supersession should have been a new dated handover linking this
one, or left to the research doc, with this file untouched.

### F-16: a memory that has grown into documentation, carrying a key the memory spec does not define
- severity: debt
- found-by: review
- batch: 5
- status: fixed
- fix: 2026-10-08. Shrunk to the three beats with no `area:` key. Its documentation already sat almost word for word in `directstorage-1-3-2026-09-12`, so `build-and-release.md` links that doc from its forwarder paragraph rather than copying it.

`.agent/memory/dstorage-dll-is-only-a-forwarder.md:8`. `spec/memory.md` says the body is three beats, the
fact, why it matters and how to apply it, and that a fact which grows into real documentation graduates
into docs and the memory file is deleted. This is 46 body lines with a four step loader procedure, a
version numbering table and two bolded gotcha sections, with neither the why nor the apply beat, and it
is the only one of the twelve memory files carrying an `area:` key. It should shrink to three beats or
graduate into `docs/systems/directstorage-streaming.md`.

### F-17: a memory file holding facts about the owner's personal kit rather than project truth
- severity: debt
- found-by: review
- batch: 5
- status: fixed
- fix: 2026-10-08. The file keeps only the project fact, the reference machine rendering on one GPU and presenting through another with no way to change that, why it mattered and how to apply it. The cable, the wiring, the other machine's owner and the quotes are gone. The same personal detail in `one-percent-lows-2026-09-14` now reads as a desktop with an RTX 5070.

`.agent/memory/reference-machine-has-no-direct-gpu-display.md:11`. `spec/memory.md` and
`.agent/README.md:42` both forbid this. The file records which cable the owner does not own, names a
desktop they own, and quotes them verbatim.

The project fact worth keeping is that the reference bench renders on one GPU and presents through
another, and that this cannot be tested away on that machine. What the owner personally owns belongs in
local agent memory, which is allowed to point into the repo and not the other way round.

### F-18: step 2 of the load order omits the load sampler start
- severity: nit
- found-by: review
- batch: 6
- status: fixed
- fix: Before 2026-10-08, by an earlier angle. Step 2 names `StartLoadSampler` beside `StartTimeline`.

`.agent/docs/foundation/proxy-architecture.md:43`. The same one shot guard that runs `ApplyFlags("late")`
and `StartTimeline` also runs `StartLoadSampler` (`proxy.cpp:444`).

### F-19: staging sizes are given four brackets and the code has three
- severity: nit
- found-by: review
- batch: 6
- status: fixed
- fix: Before 2026-10-08, by `sweep/review-render`. The paragraph gives staging its own three sizes on the 7 and 11 GB steps.

`.agent/docs/systems/directstorage-streaming.md:100` shares one bracket list between the tile pool and
the staging buffer. The tile pool does have four brackets at 7, 11 and 15 GB. Staging has three and stops
at 11 GB (`adapter.cpp:19` to 23), so a 12 GB card gets 256 MB and not the 192 the shared phrasing
implies.

### F-20: CreateQueue does two more things than the doc lists
- severity: nit
- found-by: review
- batch: 6
- status: fixed
- fix: 2026-10-08. Step 3 names the tile queue priority, and the wrapper for any of its consumers was already named by an earlier angle.

`.agent/docs/foundation/proxy-architecture.md:45`. It also overrides the tile queue's priority when
`tile_queue_priority` is not unchanged (`proxy.cpp:334`), and it wraps the queue when `logRequests` or
`streamingTrace` is on even with stats off (`proxy.cpp:351`).

### F-21: the PatchEverywhere skip list omits the mod's own module
- severity: nit
- found-by: review
- batch: 6
- status: fixed
- fix: 2026-10-08. The skip list names the mod itself first, as `iat.cpp` builds it.

`.agent/docs/foundation/proxy-architecture.md:69` gives it as kernel32, kernelbase and ntdll.
`src/core/iat.cpp:47` builds `HMODULE skip[4] = { g_self, kernel32, kernelbase, ntdll }`.

### F-22: the compile line is quoted short
- severity: nit
- found-by: review
- batch: 6
- status: fixed
- fix: 2026-10-08. The paragraph gives every flag, define and include path `build.ps1` passes, with the `/WX`, `/Brepro` and `/PDBALTPATH` the tools angle added.

`.agent/docs/ops/build-and-release.md:15` gives the flags as `/O2 /W4 /MT /std:c++17` and one include
path. `build.ps1:26` to 27 also passes /EHsc, /DUNICODE, /D_UNICODE, /DWIN32_LEAN_AND_MEAN, /DNOMINMAX
and a second include path. Everything else in that section checks out. The same doc's MIT claim at line
26 is filed against `sweep/review-public-docs` as part of its F-14.

### F-23: four bug files open on Symptom where the spec names the section Problem
- severity: nit
- found-by: review
- batch: 6
- status: fixed
- fix: 2026-10-08. All four open on Problem, and their other top level sections now sit under the four the spec names.

BUG-015, BUG-016, BUG-017 and BUG-018, each at line 14. Twenty six of the thirty use Problem.

### F-24: a semicolon in an index hook, and closed todos growing sections outside Done when
- severity: nit
- found-by: review
- batch: 6
- status: fixed
- fix: 2026-10-08. The two semicolons, on the streaming line of the Done list by then, are full stops. TODO-015, TODO-027 and TODO-007 keep their extra notes as subsections of Done when, and TODO-002's candidates and TODO-015's old status went under Done when and What the same way.

`.agent/todos/INDEX.md:43` carries the only semicolon anywhere under `.agent/`, which the shared prose
rule bans. Separately, `spec/todos.md` says completion notes go in Done when and never anywhere else, and
TODO-015 opens with an Answer section before What and adds another after Done when, TODO-027 adds a Done
section, and TODO-007 adds a Result section.

### F-25: the optimisation deep dive says the census five verdicts leaned on does not exist, where it is the load sampler's printed game code rows over the four runs of 2026-09-12
- severity: nit
- found-by: verifier
- batch: 6
- status: fixed
- fix: 2026-10-08. The doc traces 52,460 to the load sampler's printed rows over the four runs, rechecked against the logs, marks the render sampler trace as wrong with its facts kept, and keeps the point that a printed top sixteen bounds a cost and cannot zero it. Its `docs/INDEX.md` line says the same. Seen and left, its table row 2 still says the window does not include loading, against the doc's own 2026-09-14 correction.

`.agent/docs/research/optimisation-deepdive-2026-09-12.md:311` to `:313`, `:324` and `:350`, and the doc's
line in `.agent/docs/INDEX.md`. The verdicts cite 52,460 game code samples, and the load sampler's
printed game code rows add up to exactly that, 43,979 in `logs/loadsampler-20260912-1055`, 3,556 in
`logs/loadsampler-20260912-1128`, 2,506 in `logs/joblock-A-off-1446` and 2,419 in
`logs/joblock-B-on-1450`. The doc traces the figure to the deleted render thread sampler instead, whose
printed rows total 25,862, and says none of the reviewers used the load sampler. Its real point stands,
that a printed top sixteen cannot show a steady small cost, so absence from it bounds a cost rather
than zeroing it.

Raised by the verifier on batch 2 of `sweep/review-telemetry`. Left here because the doc belongs to this
angle.

## Related, filed elsewhere

`.agent/docs/systems/responsive-ui.md:13` claims every part checks the build it was read from before it
changes anything, which is false for the two vtable paths. That sentence is filed with the code fix as
`fix/review-cohtml-build-guard` F-01, because the doc is only wrong until that lands.

## Checked and clean

Every count in every index is correct: 33 docs in four categories, 12 open and 14 fixed and 4 won't fix
over 30 bug files with every status matching its group, 12 open and 12 done and 3 dropped over 27 todos
with the per area subtotals adding up, 17 standing and 3 superseded decisions with the id ranges right,
12 memory files split 8 project 1 reference 3 feedback, and 1 closed review.

Every index line points at a file that exists and every file has an index line. Zero dead links across
all 143 files, every `links:` name resolves to a real slug and every relative link resolves to a real
file. The four source paths named in docs that do not exist on disk are deliberate references to deleted
files, each naming the commit that removed them.

No secrets, no tokens, no usernames, no absolute paths with a user folder, no em dashes. Every file has
all five required frontmatter keys with the right kind and a substantive description. No decision file is
a status log, all twenty hold a Decision plus Alternatives plus Consequences, and the three superseded
ones each point at the decision that replaced them. The existing review ledger matches `spec/reviews.md`
exactly and both of its fix hashes resolve in git.

`responsive-ui.md` and `session-leak-fix.md` match the code in every checkable detail tested, down to the
RVAs, the vtable slots, the seven hashed regions and the make_shared call site. `content-package.md`,
`settings-files.md`, `package-override-layer.md`, `engine-flags.md` and `tools.md` all check out, as do
the telemetry CSV headers and the census thresholds.
