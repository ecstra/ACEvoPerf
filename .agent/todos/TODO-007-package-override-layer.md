---
name: TODO-007-package-override-layer
kind: todo
description: let loose files under the game folder replace or add package entries without repacking content.kspkg
updated: 2026-09-05
links: [directstorage-streaming, content-package, BUG-008-ui-opens-slowly-with-loading-spinner]
status: done
by: agent
area: streaming
born: 2026-09-05
done: 2026-09-05
---

## What

A `acevo_mods\` folder next to the exe whose files shadow package entries by path, served by
the proxy:

1. Table of contents: the engine reads the last 64 MB of `content.kspkg` with ordinary file I/O
   at startup (probe 2026-09-05). Hook `CreateFileW`, `ReadFile` and `SetFilePointerEx` in the
   exe's import table, recognise the package handle, and answer reads in the table range from a
   modified copy built at startup: overridden entries get the loose file's size and a virtual
   offset past the end of the package, new files get new slots (hash sorted, the table has
   139,746 free slots), the XOR flag is cleared for override entries so the engine does not
   decode them.
2. Data: every DirectStorage request is one whole entry (`FileToMemory Queue`, offset plus size
   from the table). In `QueueProxy::EnqueueRequest`, a request whose offset lies in the virtual
   range is rewritten to a `IDStorageFile` opened on the loose file (offset 0, same size). Tile
   streaming (`GpuUpload File Queue`) works the same way for plain `.texturemips` overrides.
3. Safety: the layer is off unless the folder exists, the original table is left untouched on
   disk, and the log lists every override with its virtual offset.

## Why

Every remaining UI, texture or material change (BUG-008, custom liveries, HUD tweaks) needs a
way to change package content, and repacking a 64 GB file is not it.

## Done when

A modified copy of `uiresources\menu.html` (one visible text change) in `acevo_mods\` shows in
the game, a new file added under `acevo_mods\` is readable by path, the log lists both, and the
game runs a lap with the layer on with no new errors in `acevo_perf.log`.

## Result

Built as `src/overlay/overlay.cpp`, documented in `package-override-layer`. Verified 2026-09-05:

- Replace: `menu.html` with an extra `console.log` in the head, `acevo_perf.log` shows `replace
  uiresources\menu.html`, `table rebuilt, 122398 entries used, 1 replaced`, `redirected request #1
  uiresources\menu.html +0 size 1029 -> MEMORY`, the game log shows the marker under `[gameface]`.
- Add: `acevo_overlay_test.js` referenced from the page, `add uiresources\acevo_overlay_test.js
  (83 bytes)`, `2 added`, and its marker in the game log (19:49 run).
- Lap: the 19:27 session (three stints, one override active) ran with no overlay error line.
- Two things learned on the way: the table read starts before the first `ReadFile` returns, so
  the package size must be taken at `CreateFileW` time, and kernel32's own import table must not
  be patched (its exports jump through it, the hook recursed and the game failed to start).
