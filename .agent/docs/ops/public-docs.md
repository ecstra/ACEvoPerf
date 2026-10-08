---
name: public-docs
kind: doc
description: how the readme, the changelog, the readme in the zip and the release notes are written, for players who have never seen the code, and how the next version is tracked, set on 2026-09-15 after the owner called the old ones AI slop
updated: 2026-10-08
links: [build-and-release, feedback-public-docs-are-for-players, feedback-ship-as-installable-mod, TODO-020-cut-0-3-2-after-the-three-open-investigations, reported-working-configurations]
---

# Public docs

Three files leave the repo and get read by players. `README.md` on GitHub, `CHANGELOG.md`, and
`dist/README.txt` inside the zip. The GitHub release notes and the Overtake changelog are cut from
the same text. Their reader downloaded a mod and has never seen the code.

The GitHub release notes follow 0.3.2's shape, a `## New in <version>` heading over the version's Fixed,
Added and Changed sections and nothing else. No install steps, credits or links, and no Known issues or
Game issues, which describe the mod as it stands and live in the readme. The owner set this on
2026-10-08 and keeps the Overtake changelog the same way.

On 2026-09-15 the owner read the readme and the changelog and called them AI slop and word salad.
They were written for someone who already knew the codebase, with a paragraph of mechanism behind
every fix, measurements and hedges in every changelog entry, a "what else it does" section and a
build section that was one paragraph. The mechanism belongs in the bugs, the decisions and the system
docs, and the public files carry what a player can see or do.

## Rules for all three

- Say what the player saw and what they get. A fix is the symptom in a few words, like "Blurry
  trackside big screens".
- One line per list item.
- No internals. No staging buffers, tile pools, mips, gflags, stylesheets, selectors, restyles,
  hooks, addresses or names from `src/`.
- No measurements. No milliseconds, MB/s, percentages, frame counts, node or rule counts.
- No BUG, TODO or DEC ids and no links into `.agent/`.
- No hedging. No "being honest", "not oversold", "verified", "caveat". A limit a player will notice
  is one line under Known issues.
- An ini key only where the player has to act on it, a setting that is off by default or one that
  moved. Everything on by default is covered by "any fix can be turned off there".
- The game version is the range the mod supports, `0.9+`, never a list of builds.
- Install, uninstall and build are numbered steps with one action each and the commands in code
  blocks.
- Install says copy all the files from the zip, never a list of file names. Uninstall names the
  files, because the player has to pick them out of the game folder.
- The punctuation and plain word rules of `CLAUDE.md` sections 6.3 and 7 apply here as well.

## README.md

The sections, in order.

1. Header image and badges
2. Overview, two short paragraphs, what the mod is and the cards it is reported working on
3. What it fixes, a list of symptoms
4. What it adds, a list of what a player gets by default, then the changelog link
5. Install, then Uninstall, both numbered
6. Settings, a few sentences about the ini
7. Problems, what to do and which file to attach
8. Known issues, the mod's own limits a player will notice
9. Game issues, under one line saying they are the game's own and happen without the mod too
10. How it works, one short paragraph and one or two plain sentences per fix
11. Build, numbered steps with the commands
12. Credits

There is no "what else it does" section. Anything a player gets by default goes under What it adds
and the rest lives in the ini notes.

The cards in the overview come from player reports kept in
[reported-working-configurations](../../memory/reported-working-configurations.md), and the readme line
changes only when that file does. A fix the changelog calls partial is named in What it fixes only for the part
that is fixed.

## CHANGELOG.md

- The top section is the next version, `## 0.3.2 (unreleased)` while `v0.3.1` is the last tag, or
  `## Next (unreleased)` while the owner has not named it yet, never a bare `Unreleased`.
- A version's sections are Fixed, Added, Changed, Known issues and Game issues, in that order, each only
  when it has entries. Known issues are the mod's own. Game issues are what players see that the game
  does without the mod too, said plainly as the game's under one line that says so, set by the owner on
  2026-10-08 so players stop putting the game's faults on the mod.
- One entry per user visible change, written in the same commit as the change. One sentence, or two
  when the player has to do something.
- Work that landed and came out again before a release gets no entry, and neither does a fix to
  something that was never released. The section describes what the release ships.
- Developer settings share one line per version.

## dist/README.txt

Plain text for Notepad, lines wrapped near 76 characters. Install, Settings, Uninstall and Problems
as numbered steps or a few sentences, then the Microsoft credit line naming the licence and notices
files the zip carries. Nothing a player cannot act on. The uninstall's Steam step says why it matters,
since without it the game will not start, and the update advice says to keep a changed ini.

## Versions

The next version is the owner's to name, and nothing is bumped before that (DEC-026). Once it is named,
open its changelog section and bump `ACEVO_PERF_VERSION` in `include/acevo/common.h` and the four
version fields in `src/version.rc` in one commit. Until then a user visible change still gets its
changelog line, under a `## Next (unreleased)` heading that takes the version's number when it is named.
Cutting the release swaps `(unreleased)` for the date, the steps are in
[build-and-release](build-and-release.md).

## Before and after, 2026-09-15

| before | after |
|---|---|
| badge "0.9.0 and 0.9.1" | badge "0.9+" |
| "Laggy menus, on hover, scrolling, sliders and opening pages, worst on the settings, controls and vehicle setup pages." | "Laggy menus" |
| a twenty line changelog entry on 5,978 selectors, 2,142 rules and microseconds a node | "Menu pages stuttering as they open." |
| "What else it does", eight paragraphs | gone, the parts a player gets are four items under What it adds |
| install step "Copy `dstorage.dll`, `dstorage_orig.dll`, `acevo_dstoragecore.dll` and `acevo_perf.ini` from the zip" | "Copy all the files from the zip into that folder" |
| Build as one paragraph | four numbered steps with the commands |
