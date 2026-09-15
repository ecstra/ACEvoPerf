---
name: public-docs
kind: doc
description: how the readme, the changelog and the readme in the zip are written, for players who have never seen the code, and how the next version is tracked, set on 2026-09-15 after the owner called the old ones AI slop
updated: 2026-09-15
links: [build-and-release, feedback-public-docs-are-for-players, feedback-ship-as-installable-mod, TODO-020-cut-0-3-2-after-the-three-open-investigations]
---

# Public docs

Three files leave the repo and get read by players. `README.md` on GitHub, `CHANGELOG.md`, and
`dist/README.txt` inside the zip. The GitHub release notes and the Overtake description are cut from
the same text. Their reader downloaded a mod and has never seen the code.

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
8. How it works, one short paragraph and one or two plain sentences per fix
9. Build, numbered steps with the commands
10. Credits

There is no "what else it does" section. Anything a player gets by default goes under What it adds
and the rest lives in the ini notes.

## CHANGELOG.md

- The top section is always the next version, `## 0.3.2 (unreleased)` while `v0.3.1` is the last
  tag, never a bare `Unreleased`.
- A version's sections are Fixed, Added, Changed and Known issues, in that order, each only when it
  has entries.
- One entry per user visible change, written in the same commit as the change. One sentence, or two
  when the player has to do something.
- Work that landed and came out again before a release gets no entry, and neither does a fix to
  something that was never released. The section describes what the release ships.
- Developer settings share one line per version.

## dist/README.txt

Plain text for Notepad, lines wrapped near 76 characters. Install, Settings, Uninstall and Problems
as numbered steps or a few sentences, then the Microsoft credit line. Nothing a player cannot act on.

## Versions

The version after the last tag is the next patch number unless the owner names another. Right after
a release is tagged, open the next version's changelog section and bump `ACEVO_PERF_VERSION` in
`include/acevo/common.h` and the four version fields in `src/version.rc` in one commit, so a build of
main reports the version it will ship as. Cutting the release swaps `(unreleased)` for the date, the
steps are in [build-and-release](build-and-release.md).

## Before and after, 2026-09-15

| before | after |
|---|---|
| badge "0.9.0 and 0.9.1" | badge "0.9+" |
| "Laggy menus, on hover, scrolling, sliders and opening pages, worst on the settings, controls and vehicle setup pages." | "Laggy menus" |
| a twenty line changelog entry on 5,978 selectors, 2,142 rules and microseconds a node | "Menu pages stuttering as they open." |
| "What else it does", eight paragraphs | gone, the parts a player gets are three items under What it adds |
| Build as one paragraph | four numbered steps with the commands |
