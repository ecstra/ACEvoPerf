---
name: BUG-031-the-heap-still-grows-about-5-mb-a-visit-with-the-session-leak-fix
kind: bug
description: with the session leak fix freeing every finished session, the game's live heap still grows about 5 MB with each identical Red Bull Ring visit, what grows not named yet
updated: 2026-09-16
links: [BUG-016-vram-overhead-grows-across-scene-loads, session-leak-fix, session-leak-census-2026-09-16, TODO-023-name-what-the-game-keeps-across-identical-loads]
status: open
severity: nit
area: stability
reported: 2026-09-16
parent: BUG-016-vram-overhead-grows-across-scene-loads
---

## Problem

Owner wording, 2026-09-16, listing what is left after the session leak fix: "the remaining mem leak".

With `session_leak_fix=1` every finished practice and menu session is freed, and the game's live heap still
reads a few MB higher after every identical visit, where the leak before the fix was about 57 MB a visit.

## Evidence

`logs/sessionfix-rbr-20260916`, the memory census's settled menu readings after each of six Red Bull Ring
practice visits parked in the pit box.

| after visit | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| heap in use, MB | 3740 | 3748 | 3752 | 3755 | 3758 | 3765 |
| commit, MB | 8749 | 8951 | 8991 | 9061 | 9066 | 9075 |

Steps of 8, 4, 3, 3 and 7 MB of live heap, and commit steps falling from 202 MB to 5 and 9 MB. Fifty visits
would add about 250 MB. The run took no dumps, so what grows is not known. The census run before the fix
([session-leak-census-2026-09-16](../docs/research/session-leak-census-2026-09-16.md)) attributed its growth to
whole sessions and did not look for anything smaller.

## Fix

Absent. Naming it takes the TODO-023 route again with the fix on, a dump after the second visit and one after
the seventh, diffed by class and block size the same way, leaving out the one session each dump still holds.

## Verification

Absent.
