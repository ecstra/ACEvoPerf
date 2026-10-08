---
name: BUG-023-overlay-keeps-a-64-mb-table-copy-for-the-whole-session
kind: bug
description: the package override layer keeps its 64 MB decoded copy of the package table for the whole session, although the game reads its table in the first seconds, 64 MB of commit the mod holds for nothing, fixed by keeping only the replaced slots when nothing is added
updated: 2026-10-08
links: [package-override-layer, memory-creep-2026-09-14, BUG-016-vram-overhead-grows-across-scene-loads]
status: fixed
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

Fixed on 2026-10-08 on `fix/overlay-table-memory`, in the shape the dive proposed. A table read already
comes back from the package with the package's own bytes, which match the edited table everywhere but the
replaced slots, so when no override adds an entry only those slots are kept and copied into the part of
each read they cover, and the 64 MB vector is released once the table is built. An added entry shifts
every slot after it, so then the whole table is kept as before. The failure paths that read the table and
gave up now release it too. The hash compare the dive suggested is not needed, since the bytes around the
edited slots are the package's own.

## Verification

Owner driven on 2026-10-08, `logs/overlay-table-20261008`: the log says the copy was freed, both of the
mod's own corrections were found through the edited slots and served (`redirected request #1` for the UI
stylesheet, `#2` for the big screen flipbook), the menus stayed smooth, and the process commit settled in
the menu at 8,096 MB against 8,152 to 8,202 MB in three runs that day without the fix, about 60 MB lower
from the first second.
