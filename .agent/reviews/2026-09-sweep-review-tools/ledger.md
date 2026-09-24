---
name: review-2026-09-sweep-review-tools
kind: review
description: the tooling angle of the full review of main, a package extract that can write outside its output folder and a row of parsers that produce a wrong file at exit 0, eighteen findings and one added by another angle's hunter, one breaks
updated: 2026-09-24
links: [spec-reviews, house-rules-agent, tools, build-and-release, reviews-index]
branch: sweep/review-tools
status: open
---

# Review of the Python tools, the build and the release scripts

## Summary

One angle of the full review of main run on 2026-09-20 at high. This angle covers `tools/*.py`,
`build.ps1`, `release.ps1` and `.gitignore`. Two reviewers worked this surface and verified several
claims with throwaway experiments rather than by reading: the MSVC duplicate basename behaviour,
`statistics.correlation` on constant input, the `os.path.join` escape and the protobuf short slice.

One theme runs through the Python: a length or an offset is read out of a file and used without a
check, and the failure is silent. Every one of these ends with the tool printing success and writing
something that is not what it read.

Eighteen findings, one breaks, five bug, ten debt, two nit. F-19 was added on 2026-09-24 by the
hunter of `sweep/review-overlay`, whose named slot constants point readers at `parse_slot` here.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | a tool cannot write outside where it was told to | pending | |
| 2 | a truncated or odd input fails loudly instead of producing a wrong file | pending | |
| 3 | a write to the game's own files is always recoverable | pending | |
| 4 | the build and the release check what they ship | pending | |
| 5 | the tools read the way the house style says | pending | |

## Findings

### F-01: extract builds its output path from the package's own path strings, so a crafted package writes anywhere
- severity: breaks
- found-by: review
- batch: 1
- status: open
- fix:

`tools/kspkg.py:145`. `out = os.path.join(a.out, e.path)` where `e.path` comes from the TOC slot, and
`parse_slot` only checks that the bytes are printable ASCII, which allows both a parent directory
escape and a drive letter. Verified in the session:
`os.path.join("extracted", r"C:\Windows\Temp\evil.dll")` returns the absolute path and discards the
output folder entirely. `os.makedirs` then `open(out, "wb")` writes there.

Failure: the repo itself documents the package format and a loose mods folder, so someone shares a
modified content.kspkg. The owner runs an extract over it and files land next to the game exe or in the
startup folder. Harmless against the shipped package today, and `-p` accepts any file. One
`os.path.realpath` containment check before the open closes it.

Found independently by two reviewers.

### F-02: read_entry seeks to a TOC offset and reads a TOC size with no check against the file size, so a short read silently produces a truncated file at exit 0
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`tools/kspkg.py:97`. Only the first slot's offset plus size is checked against the file size, at line
77. For every other entry `f.seek(e.offset)` past end of file succeeds in Python, `f.read` returns
empty, the loop breaks, and `cmd_extract` writes a zero byte or half length file while printing
"extracted N file(s)". `cmd_cat` does the same to stdout with exit code 0, which is exactly what
`texture_mips.kspkg_cat` trusts, since `check=True` only catches a nonzero exit.

### F-03: a protobuf length prefix is used as a slice bound without checking it fits, so a truncated header is re emitted as a valid looking but different message
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`tools/texture_mips.py:95`. Python slicing does not raise past the end, so `data[at : at + size]`
silently yields fewer bytes than `size` claims and `at += size` runs the loop out. `emit` then writes
`write_varint(len(blob))` from the short blob, so `strip` produces a `.texture` that parses cleanly and
is not the file that was read.

Failure: chained with F-02, a bad TOC offset gives a short header at exit 0 and the tool writes a
malformed override asset into the folder the game loads from.

### F-04: a descriptor that fails to enter the pool is swallowed with a bare pass and still marked as added
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`tools/protodesc.py:41`. `pool.Add(fd)` raises on a duplicate symbol or a conflicting redefinition, the
exception is bound to a name and discarded, and `added.add(n)` runs anyway, so `load` returns a pool
missing that file. The sibling handler five lines below at 46 runs the same call and prints "pool add
fail", so exactly the failures the author thought worth reporting are discarded on the path that
normally runs. Lines 14, 23 and 32 do the same with `except Exception: break` and
`except Exception: continue`.

Failure: `acevo_settings.py` resolves `VideoSettings` against whatever definition did land, parses the
settings file with it and writes the result back in `cmd_set`. The only thing between that and a
corrupt `video.videosettings` is the backup copy. It surfaces to the user as "unknown field", pointing
at the wrong cause.

Found independently by three reviewers.

### F-05: restore overwrites the live settings file without backing it up first, breaking the tool's own documented rule
- severity: bug
- found-by: review
- batch: 3
- status: open
- fix:

`tools/acevo_settings.py:140`. The module docstring and `.agent/docs/ops/tools.md` both say every write
copies the file to a timestamped backup first. `cmd_set` does. `cmd_restore` calls `shutil.copy2`
straight over the current file, and with no argument it picks the newest backup.

Failure: restore the wrong one and the state you were in is gone with nothing to go back to. `main`
also skips the `--file` existence check for restore at line 160, so a bad path gives a raw traceback
rather than a message.

### F-06: statistics.correlation raises on a constant input, killing the report just before the sections worth reading
- severity: bug
- found-by: review
- batch: 4
- status: open
- fix:

`tools/telemetry_report.py:254`. A session with the GPU clock pinned, a locked laptop clock or a menu
idle window where the reported clock never moves, gives pairs with zero variance on one axis.
`statistics.correlation` raises `StatisticsError` and it propagates out of `main`, so the slow clusters,
the logged hitches, the PSO counts and the game log errors never print. The same happens on the frames
axis for a session pinned at a frame cap. The `len(pairs) > 3` guard does not cover it. This is the
edge case in this file most likely to actually bite.

### F-07: the TOC loop stops at the first slot that fails validation, so one odd entry silently shortens every listing and extraction
- severity: debt
- found-by: review
- batch: 2
- status: open
- fix:

`tools/kspkg.py:83`. `parse_slot` returns None for any path with a byte outside the printable range,
which is the intended stop for the zero padded tail and also fires on the first localised or non ASCII
path a future package build carries. The loop cannot tell an empty slot from one it could not decode.

Failure: `info`, `list`, `stats` and `extract` all report only the entries before it, `cat` answers "not
found" for everything after it, nothing says the table was cut short, and `verify` then confirms the
truncated set as clean. `texture_mips.py` shells out to `cat` for its headers, so it inherits the same
silent miss.

### F-08: every object goes into one flat output directory, so two sources with the same base name silently collide
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`build.ps1:27`. Verified with the same toolchain: compiling two same named sources from different
folders into one output directory exits 0, prints no warning and leaves one object file. The project's
own convention is one folder per concern with names like `stats.cpp`, `log.cpp` and `process.cpp`, so a
future `src/render/stats.cpp` beside `src/dstorage/stats.cpp` drops one translation unit. The link
usually fails with an unresolved external, and a file whose work is a self registering object or a hook
installed at static init simply disappears from the DLL. No collision exists today.

### F-09: the release version comes from a free text field and nothing checks the changelog or the ini it is about to ship
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`release.ps1:6` reads `VersionInfo.FileVersion`, the `StringFileInfo` value, which is independent of the
numeric `FILEVERSION` two lines above it in `src/version.rc` and of `ACEVO_PERF_VERSION` in
`include/acevo/common.h`. All three agree today.

What is missing is the two checks either side of it. The script verifies the five payload files exist
and does not check that every `[developer]` key in `dist/acevo_perf.ini` is 0, and does not check that
the top CHANGELOG heading carries a date rather than "(unreleased)". Running it right now would produce
a shippable 0.3.3 zip against an empty unreleased changelog section and say nothing, and a stray
`timeline=1` left in the ini would ship and write a CSV into every player's game folder.

### F-10: the default output folders of two tools are not ignored, so decoded game content lands in the working tree ready to be committed
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`.gitignore`. `kspkg.py extract` writes into `.\extracted\` by default and `git check-ignore` confirms
it is not ignored, so the next `git add -A` sweeps the game's own assets into a public repo. The release
doc already records an Overtake takedown over a file that was byte identical to a game file.
`texture_mips.py:228` has the same shape, with `TEMP` unset its scratch folder becomes
`.\acevo_texture_mips\`, also unignored, and neither the header nor the payload it leaves behind is ever
cleaned up.

Also in this batch: `CONTRIBUTING.md:21` promises that secrets live in a gitignored root `.env` and
`.gitignore` has no `.env` rule at all, so somebody following the documented rule stages it with no
warning.

### F-11: /W4 with no /WX and a documented accepted warning means a new warning in the mod's own code passes the gate unnoticed
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`build.ps1:26`. The build gate is "zero errors", and `build-and-release.md` records one expected C4244
from inside the STL. A real truncation warning in the mod's own code scrolls past in the same output and
the script's only check is the exit code. The engine branch's F-07 closes the one warning currently in
the way, which makes `/WX` possible.

The output is also not reproducible. There is no `/Brepro`, the build uses `/DEBUG:FULL`, and the link
order follows a directory enumeration, so two builds of identical source differ.

### F-12: every shipped dstorage.dll carries the build machine's absolute path
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`build.ps1:29`. `/DEBUG:FULL` plus `/PDB` writes the debug path into the PE, confirmed present in the
current `dist/dstorage.dll`. No username in it today, and this is the exact channel that publishes one
the moment anyone builds a release from a folder under a user profile. The project rule about never
writing a machine path into an artifact has no guard here. `/PDBALTPATH:%_PDB%` keeps the symbols usable
and ships only the file name.

### F-13: two of the six tools carry no type annotations while two annotate everything
- severity: debt
- found-by: review
- batch: 5
- status: open
- fix:

`tools/acevo_settings.py:68` has none anywhere, and `tools/kspkg.py` is half done. `telemetry_report.py`
and `texture_mips.py` are fully annotated, so a reader cannot tell from the folder which contract is
real. The same two files also pack a parser, its arguments and its defaults into single semicolon chains
at `kspkg.py:197` to 199 and `acevo_settings.py:151` to 154.

### F-14: protodesc.py is the one file in tools with no docstring, no annotations, one letter names and semicolon packed lines
- severity: debt
- found-by: review
- batch: 5
- status: open
- fix:

`tools/protodesc.py:3`. Its five siblings follow the house style and this one breaks naming, typing and
spacing together. Line 27 opens a file with no context manager where every other tool uses `with`. It is
also the file `acevo_settings.py` depends on for its whole schema, so it is the one a reader has to
follow to understand any settings failure.

### F-15: five type ignore comments all paper over one loose type alias
- severity: debt
- found-by: review
- batch: 5
- status: open
- fix:

`tools/texture_mips.py:115`, and again at 136, 157, 160 and 162. `Field = tuple[int, int, object]` at
line 36 throws away what the third element actually is, and each ignore asserts at a call site what the
alias could have said once. `int | bytes` is the real type, visible from `parse` itself, and it would
delete all five comments. This file is otherwise the most careful in the folder, it validates the tile
arithmetic against the payload length at line 183 before writing anything.

### F-16: two parsed fields in the report are built for every row and never read
- severity: debt
- found-by: review
- batch: 5
- status: open
- fix:

`tools/telemetry_report.py:105`, the streamer Counter, is filled by a lowercase substring scan at line
118 against every timestamped line of a multi megabyte game log and is never referenced after the unpack
at line 182. `GpuSample.reasons` at line 27 is parsed out of every nvidia-smi row at line 94 and never
read, and the "throttle reasons active" line at 250 prints three other fields instead. A reader of the
docstring at line 103 expects both to appear in the report.

### F-17: one letter locals throughout a file whose signatures are otherwise exemplary
- severity: nit
- found-by: review
- batch: 5
- status: open
- fix:

`tools/telemetry_report.py:172`. Every function is annotated and then the bodies read as single letters.
`num = lambda s: ...` at line 88 is an unnamed callable rebuilt on every csv row inside `read_gpu`.

### F-18: half the shipped profiles are invisible in the tool's own usage text
- severity: nit
- found-by: review
- batch: 5
- status: open
- fix:

`tools/acevo_settings.py:12`. `PROFILES` at line 28 holds four entries and the docstring that `--help`
is built from names only two, so a user has to run the separate `profiles` subcommand to learn that
`pacing` and `gpu-relief` exist.

### F-19: kspkg.py's docstring gives a table slot two pad bytes it does not have
- severity: nit
- found-by: hunter
- batch: 5
- status: open
- fix:

`tools/kspkg.py:8` lays the slot out as
`path[0xE4] | u8 pad | u8 pad | u16 flags | u16 pathlen | u64 hash | u64 size | u64 offset`, which
adds up to 258 bytes and puts the flags at 0xE6. Its own parenthetical, `parse_slot` at line 60,
`content-package.md` and the override layer's named constants all put the flags at 0xE4 and the
path length at 0xE6, with no pad.

Failure: someone checks the override layer's `SLOT_FLAGS` against the tool, reads the docstring
rather than `parse_slot`, and finds the field two bytes off.

Raised by the hunter on batch 3 of `sweep/review-overlay`. Left here because the file belongs to
this angle.

## Checked and clean

No tool hardcodes the game folder. `gamedir.py` reads `ACEVO_GAME_DIR`, and `acevo_settings.default_file`
builds from `%USERPROFILE%`, which is the documented location of the game's own settings and not the
game folder. No file path in scope contains a username. `texture_mips.py` checks its tile arithmetic
against the real payload length before writing, and its one subprocess call passes an argument list with
no shell.
