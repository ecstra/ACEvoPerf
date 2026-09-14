---
name: BUG-023-overlay-keeps-a-64-mb-table-copy-for-the-whole-session
kind: bug
description: the package override layer keeps its 64 MB decoded copy of the package table for the whole session, although the game reads its table in the first seconds, 64 MB of commit the mod holds for nothing
updated: 2026-09-14
links: [package-override-layer, memory-creep-2026-09-14, BUG-016-vram-overhead-grows-across-scene-loads]
status: open
severity: debt
area: streaming
reported: 2026-09-14
parent: memory-creep-2026-09-14
---

## Problem

`src/overlay/overlay.cpp` builds `g_toc`, a `std::vector<BYTE>` the size of the package table
(0x4000000 bytes, 64 MB), on the first table read and keeps it until the process exits. It is only read
by `PatchTableRead`, which copies from it into the game's buffer when a read falls inside the table
range.

## Evidence

From the BUG-016 deep dive of 2026-09-14. The mod's fixed commit over the passive run is 66 to 71 MB,
and 64 MB of it is this vector (`overlay.cpp`, the resize in `BuildToc`). In
`logs/memcreep-20260913/S-census-settled` all 52 package opens read their tables within 2 s of attach.
Whether any later read touches the table range was not checked, so the fix has to keep serving it.

## Fix

Absent. The shape proposed by the dive is to keep only the slots that differ from the package, the
replaced and added entries, and serve the rest of a table read from the file itself, falling back to
the full copy when an added file inserts a slot and shifts the sorted table. A hash compare of the
served range guards it.

## Verification

Absent. One normal launch with an override present, logging private commit before and after the table
build, and the log still showing the overlay's redirected request lines.
