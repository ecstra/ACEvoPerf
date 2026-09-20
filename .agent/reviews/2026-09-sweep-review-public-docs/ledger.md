---
name: review-2026-09-sweep-review-public-docs
kind: review
description: the player facing angle of the full review of main, an uninstall that can leave the game unable to start and settings whose comments do not say what they turn off, sixteen findings, one breaks
updated: 2026-09-20
links: [spec-reviews, house-rules-agent, public-docs, build-and-release, reviews-index]
branch: sweep/review-public-docs
status: open
---

# Review of the player facing documents and the shipped release

## Summary

One angle of the full review of main run on 2026-09-20 at high. This angle covers `README.md`,
`CHANGELOG.md`, `dist/README.txt`, `dist/acevo_perf.ini` read as a document, `CONTRIBUTING.md`,
`LICENSE` and the contents of `release/ACEvoPerf-0.3.2.0.zip`.

The release mechanics are in good shape. The zip holds exactly the five files the script builds, its
ini is byte identical to the source ini with every developer key off, and every version number agrees
across the dll, the zip name, the tag, the changelog and the two version files.

What the review found is a gap between what a setting's comment promises and what turning it off
actually does, an uninstall sequence a player can follow into a game that will not launch, and two
redistribution questions.

Sixteen findings, one breaks, four bug, nine debt, two nit.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the uninstall cannot leave a broken game | pending | |
| 2 | every setting comment names what turning it off costs | pending, F-03 and F-04 already closed by sweep/review-render | |
| 3 | the readme and the changelog agree with each other and with the trackers | pending | |
| 4 | what we redistribute carries what it has to carry | pending | |
| 5 | tone and the leftovers | pending | |

## Findings

### F-01: the uninstall steps can leave the game unable to start
- severity: breaks
- found-by: review
- batch: 1
- status: open
- fix:

`dist/README.txt:22`. Step 2 says to delete dstorage.dll, dstorage_orig.dll and everything starting with
acevo_. Step 3 says only "In Steam, right click the game, then Properties, Installed Files and Verify
integrity of game files", with no reason given.

Failure: the game imports `DStorageGetFactory` statically from dstorage.dll, so a player who reads step
3 as optional tidying has an exe that will not load at all. `README.md:63` gets this right with "This
puts the game's own dstorage.dll back". The zip readme needs the same clause on step 3.

Verified by me against the file on 2026-09-20.

### F-02: the update advice silently wipes the player's own settings
- severity: bug
- found-by: review
- batch: 1
- status: open
- fix:

`README.md:57` says "After a game update, copy the files in again", and `dist/README.txt:15` says the
same immediately after step 4, "When Windows asks, replace the file". The zip carries
`acevo_perf.ini`.

Failure: a player who set `reflex_boost=1` and a smaller staging size follows this exactly and has both
reset to the shipped defaults with no warning and nothing to restore from. `build.ps1:50` does the
opposite for the owner's own installs, copying the ini only when none exists and printing "existing ini
kept", so the project already knows this matters. Every key falls back to its code default when absent,
so keeping the old ini works.

Found independently by three reviewers.

### F-03: the dxgi enabled comment understates what turning it off costs
- severity: bug
- found-by: review
- batch: 2
- status: fixed
- fix: 2b5a3aa, 2026-09-20, on `sweep/review-render` with the code half of it.

`dist/acevo_perf.ini:52` reads "leave at 1, needed to measure frame times". Setting it to 0 also removes
the automatic staging buffer, the automatic tile pool and Reflex, which is the render branch's F-04. The
comment has to name the memory sizing and Reflex, not only frame timing.

Closed by the render branch, which owns the file this finding is about, so nothing is left here to do.
The comment now names Reflex and the frame times, and the log says what the setting takes down. The
memory sizing half of this finding stopped being true in between: `ResolveAutoSizesFallback`, added on
that branch, reads the card at the first DirectStorage call and never consults `dxgiEnabled`, so
`enabled=0` no longer costs the staging buffer or the tile pool at all. A comment naming the memory
sizing would now be wrong.

### F-04: frame_stats=0 silently turns off NVIDIA Reflex and the comment does not say so
- severity: bug
- found-by: review
- batch: 2
- status: fixed
- fix: 4982e86 then the batch 2 hunter round, 2026-09-20, on `sweep/review-render`.

`dist/acevo_perf.ini:53` reads "1 measures how long each frame takes", while `[latency] reflex=1` two
sections away reads as if Reflex is independent. It is not, which is the render branch's F-05. Either
the comment names the dependency or the Reflex install moves out from behind it, and the doc change
follows whichever the code does.

The render branch took the second option, so Reflex is independent now and the comment says which
things still are not. The hunter on that batch found the rest of the same shape: `[log] hitch_ms` and
the two developer CSVs have no switch of their own and go quiet with `frame_stats=0`, which the comment
and a new log line now both name.

### F-05: a default two users reported as breaking night lighting ships on with nothing said anywhere
- severity: bug
- found-by: review
- batch: 3
- status: open
- fix:

`dist/acevo_perf.ini:36` ships `enable_pso_cache=true` with the comment "true means fewer shader
stutters", and `README.md:42` advertises it. BUG-015 records two independent Overtake reports on 0.3.1
that at night the car's headlights light nothing at Oulton Park and no trees at the Nurburgring, and
that setting it false cures it. The owner closed that wontfix on 2026-09-15 and put the flag back on,
because nothing on the reference machine reproduces it.

That is a defensible call and it is one the documents then have to carry. `CHANGELOG.md:37` Known issues
says nothing about it and the ini comment gives no hint, so a player seeing dark trees at night has no
way to find the switch. `public-docs.md:33` asks for the ini key exactly where the player has to act on
it. One Known issues line naming the key.

### F-06: the readme claims a fix the changelog says is only partial
- severity: debt
- found-by: review
- batch: 3
- status: open
- fix:

`README.md:31` lists "Blurry cars in races with AI" flat, while `CHANGELOG.md:41` says under Known
issues that in a full race AI cars and some trackside buildings can still look blurry. BUG-020 agrees.
README has no Known issues section by design, so a player who only reads the readme gets a claim the
changelog contradicts. Narrowing the line to what actually got fixed, the player's own car staying sharp
near other cars, is the fix.

### F-07: an open freeze from the release day is missing from Known issues
- severity: debt
- found-by: review
- batch: 3
- status: open
- fix:

`CHANGELOG.md:37`. The 0.3.2 Known issues section lists four items, all about menus, frame pacing and
textures. BUG-032 is open, reported 2026-09-18, the same date as the 0.3.2 heading, and is the most
visible thing a player can hit in this release. It is not proven to be the mod's fault, which is a reason
to word it carefully and not a reason to leave it out.

### F-08: the How it works section carries measurements the rulebook bans
- severity: debt
- found-by: review
- batch: 5
- status: open
- fix:

`README.md:77` says the game sets aside about 2 GB of video memory for loading, which leaves a 6 GB card
short, and line 78 does the same for textures. `public-docs.md:28` is flat about it, no measurements, no
milliseconds, no MB/s, no percentages. The same paragraph works without the numbers. Everything else in
the three player files is clean on this.

### F-09: the release script never checks the ini it is about to ship
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`release.ps1:11`. Recorded here as the doc side of the tools branch F-09, because the thing it fails to
check is a player facing file. The 0.3.2 zip is clean, so this is a missing guard rather than a live
defect.

### F-10: diagnostic output is on by default outside the developer section
- severity: debt
- found-by: review
- batch: 2
- status: open
- fix:

`dist/acevo_perf.ini:18` ships `stats=1` with `stats_interval_s=10`, so every player gets a loading
telemetry line in acevo_perf.log every ten seconds for the whole session. `CHANGELOG.md:34` tells players
"Diagnostic settings moved to a new [developer] section", which reads as if all of them did. Either this
belongs in `[developer]` off by default, or the comment says plainly that it is what the log the Problems
section asks for is made of. Note that the proxy branch's F-01 means turning it off currently takes the
whole override layer with it, so the code fix has to land first.

### F-11: three shipped knobs whose own comments tell the player never to change them
- severity: debt
- found-by: review
- batch: 5
- status: open
- fix:

`dist/acevo_perf.ini:9` to 11: `min_queue_capacity` ("leave at 0, the game already sets the maximum"),
`submit_threads` ("leave at 0 to let DirectStorage choose") and `cpu_decompression_threads` ("leave at 0,
the game's files are stored uncompressed"). Each is parsed, each reaches exactly one consumer, and all
three are no ops at the shipped value. CLAUDE.md says the mod corrects engine misbehaviour and never
ships settings knobs.

### F-12: two settings cannot do anything at their shipped defaults and the branches they guard never run
- severity: debt
- found-by: review
- batch: 5
- status: open
- fix:

`dist/acevo_perf.ini:17`, `tile_queue_priority=unchanged`, maps to a sentinel that makes its only
consumer's branch unreachable. `dist/acevo_perf.ini:48`, `clear_xor_flag=1`, leaves both of its arms in
overlay.cpp unentered. Either they earn a reachable default or they leave the ini.

### F-13: a compatibility claim with no source in the repo
- severity: debt
- found-by: review
- batch: 3
- status: open
- fix:

`README.md:23` says players have reported it working on RTX 2060, 3060 Ti, 3070 Ti, 4050 and 4060 cards.
Nothing under `.agent/` records those reports, and BUG-015 shows that Overtake reports do get filed when
they arrive. If the list came from listing comments it needs a note somewhere in `.agent/` so the next
person can check it before the next release repeats it.

### F-14: Microsoft's licence and notices are not carried with the redistributed binaries
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

Two of the five files in the zip are Microsoft's DirectStorage 1.3.0 runtime. The only mention of their
terms is `dist/README.txt:35`, a one line credit with no copy of the text and no link.
`third_party/directstorage/LICENSE.txt` section 2(b)(ii) requires the distributor to bind external end
users to terms that protect Microsoft at least as much, and NOTICES.txt is a third party notices file
covering further components. Adding both to the payload array, or at minimum a URL in the zip readme,
is the whole fix.

Related and in the same batch: `tools/data/proto_schema.txt` and `tools/data/gflags_full.tsv` are
verbatim extracts from the game exe, down to internal source file names and help strings, sitting in a
public repo under a root LICENSE that claims MIT over the whole tree, which it cannot grant for Kunos
material. This is the largest takedown surface the repo has. A README note carving out `tools/data` and
`third_party`, or generating the schema locally instead of committing it, both work.
`.agent/docs/ops/build-and-release.md:26` also calls the package MIT, which is true only of the code.

Found independently by two reviewers.

### F-15: CONTRIBUTING.md has a broken wrap and three punctuation breaches
- severity: nit
- found-by: review
- batch: 5
- status: open
- fix:

`CONTRIBUTING.md:3`. Lines 3 to 5 wrap into a dangling fragment that ends on a line holding only
"behavior." Line 9 has a mid sentence colon, line 18 has a hyphenated compound that is not an
established one, and line 5 spells "behavior" while the rest of the repo uses British forms including
"licence" at README.md:13 and :111.

### F-16: one developer setting does not name the file it writes
- severity: nit
- found-by: review
- batch: 5
- status: open
- fix:

`dist/acevo_perf.ini:62`. Every other CSV producing key in the section names its file. This one writes
`acevo_perf_load_samples.csv` and does not say so.

## Checked and clean

The zip holds exactly the five files `release.ps1` builds, nothing extra, no PDB, no logs, flat at the
root so "copy all the files from this zip into that folder" works as written. Its ini is byte identical
to `dist/acevo_perf.ini` and all eleven `[developer]` keys are 0. Every version agrees: the dll inside
the zip reports 0.3.2.0, the zip name matches, the tag exists, the changelog dates 0.3.2 to 2026-09-18,
and `src/version.rc` with `include/acevo/common.h` are both bumped to 0.3.3 with an open unreleased
heading, which is what `public-docs.md` asks for.

Every ini default matches `src/core/config.cpp` across all thirty odd keys. Every fix named in the readme
traces to a fixed bug in the tracker and to live code. `third_party/directstorage` is the real Microsoft
NuGet 1.3.0 redistributable, both binaries report ProductVersion 1.3.0, and `distributable_files.txt`
lists exactly the two files that ship.

`README.md`, `CHANGELOG.md`, `dist/README.txt` and `dist/acevo_perf.ini` are clean on punctuation: zero em
dashes, zero semicolons, zero mid sentence colons, and the only hyphens are inside badge URLs.
