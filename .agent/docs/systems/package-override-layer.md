---
name: package-override-layer
kind: doc
description: how loose files under acevo_mods replace or add entries of content.kspkg at run time
updated: 2026-09-23
links: [content-package, directstorage-streaming, proxy-architecture, TODO-007-package-override-layer, TODO-009-overlay-serves-copies-so-loose-files-stay-editable, BUG-017-trackside-big-screens-blurry, responsive-ui]
---

# Package override layer

Files under `<game>\acevo_mods\<package path>` shadow entries of `content.kspkg`. The package on
disk is never written. Code: `src/overlay/overlay.cpp`, ini section `[overlay]`.

## How the engine reads the package

- At startup the engine opens the package through the C runtime (`ucrtbase.dll`, `fopen`) and
  reads the last 64 MB, the table of contents, in 4 KB `ReadFile` calls starting at the table
  offset. It then opens and closes the package over and over through the same C runtime and reads
  some entries on those handles in the same 4 KB pieces, 132 opens and 131 distinct offsets outside
  the table in the traced run of 2026-09-05.
- Everything else goes through DirectStorage. Every request is one whole entry: offset and size
  come straight from the table. Small files (text, scripts, data) travel on the `FileToMemory
  Queue` (destination `MEMORY`), textures on the `GpuUpload File Queue` (destination `TILES`).
- An entry whose table flag bit 8 is set is XOR ciphered (see `content-package`). Clearing the bit
  makes the engine take the data as is: an overridden text file was served plain and used.

## What the layer does

1. `Install` (DllMain): collect the loose files (recursive, paths lower cased, backslashes), hook
   `CreateFileW/A/2`, `ReadFile` and `CloseHandle` in the import tables of every module except
   kernel32, kernelbase and ntdll (`PatchEverywhere`). Handles opened on `content.kspkg` are
   remembered, the package size and table offset are taken from the first one.
2. First table read: `BuildToc` reads the real table through the original functions, decodes it,
   and for every loose file finds the slot by FNV-1a 64 hash (binary search, the table is sorted).
   An existing entry gets the loose file's size and a virtual offset, bit 8 cleared. A new path
   gets a slot inserted in hash order (the table has 139,746 free slots). Virtual offsets start at
   the package size rounded up to 64 KB and are 64 KB aligned. The table is re encoded and every
   later read in the table range is answered from this copy.

   It runs once, under `std::call_once`, so a second thread reading the table meanwhile waits for
   the copy rather than building its own or getting its piece unedited. It uses the sizes step 1
   took and leaves the table alone if the package it opens is any other size.
3. Reads at a virtual offset (`Hook_ReadFile`) are served from the loose file and the file
   position is advanced as if the package had the data, with or without an `OVERLAPPED` giving the
   offset. On a handle opened with `FILE_FLAG_OVERLAPPED` they go to the package unchanged instead,
   said once, and get end of file through the caller's own event or completion port. A completion
   made up in the hook set the event but queued no completion packet, so a caller bound to a port
   waited forever. A loose file that cannot give all the table promised, missing, shorter or held
   by another program, is said the first time, and a read it cannot answer at all goes to the
   package, which past its end answers end of file in whichever shape it was asked. No traced run
   has served a file this way yet, but it is a live path, since the engine reads some entries with
   plain file reads as above and an override of one of those comes through here.
4. DirectStorage requests whose offset is virtual (`OverlayRedirect`, called from
   `QueueProxy::EnqueueRequest`) get their source swapped to an `IDStorageFile` opened on the
   loose file, offset rebased. The file stays open for the life of the process, so a loose file
   that has been requested once cannot be edited while the game runs.

   A loose file that cannot be opened when the table is built is left out of it, opened the way
   DirectStorage opens it so a file another program is still writing fails there too, and its size
   is taken from the open file rather than the folder listing, which gives 0 for a symbolic link.
   Left out, a replaced entry is read from the package, or from the mod's own correction for it.

   One that opened then and fails its first DirectStorage open later, quarantined, deleted or
   locked in between, is said once and not retried. Its slot already points past the end of the
   package, so DirectStorage fails every request for that entry for the session without writing
   the buffer, and the fence after it still fires. The game hears of it only through a status
   array, which it made in none of 118 captured runs, or the queue's error record, and unless it
   reads that it takes whatever the buffer held. With no factory to open a file with at all, the
   same happens to every replaced entry, said once. The mod's own two files are written after the
   startup check, so for them only this later path applies.

   That one call site is why `overlay::Active()` is in the condition that decides whether a queue
   is wrapped at all. Until 2026-09-20 the wrapper existed only for the statistics and the two
   developer traces, so `[directstorage] stats=0` took this whole step with it while the install
   and table lines still printed as though the layer were running.

## The mod's own corrections

The layer also serves files the mod generates at the first table read from the player's own package,
with no `acevo_mods` folder needed: the big screen flipbook header with one mip level (`AddBigScreenFix`,
`acevo_bigscreen.texture`, BUG-017) and the UI stylesheet with seven hover and focus selector parts narrowed
(`AddUiStyleFix`, `acevo_uicomponents.css`, part of [responsive-ui](responsive-ui.md)). Each is written next
to the exe and pushed as an override like a loose file, and each is skipped with a log line when the
package entry is not the size or shape it expects.

A player's own file for either entry in the mods folder wins, and the correction is skipped with a
log line. Until 2026-09-23 the correction joined the list after the player's file and took the slot,
so the player's file was never served and the log showed the entry replaced twice.

## Verified on 2026-09-05

A 1 KB text file of the package replaced by a marked copy, plus a new 83 byte file added next to
it: `acevo_perf.log` shows `replace ... (1029 bytes) -> virtual offset 69070749696`, `add ...
(83 bytes)`, `table rebuilt, 122398 entries used, 1 replaced` (later `2 added`), `redirected
request #1 ... +0 size 1029 -> MEMORY`, and the game log shows the markers from both files. Table
rebuild costs 80 ms at startup (64 MB read and decoded once).

## Limits

- Only the 64 MB table of 0.9.0 and 0.9.1 is read. A package whose table is not recognised, such
  as the 32 MB one the public package tools read, is left alone with a line in the log.
- Paths longer than 223 bytes are skipped with a log line. The path field of a slot runs to 227
  bytes and a NUL, and the layer keeps four bytes spare.
- The `.texturemips` tile files are streamed by 64 KB tile, an override must keep the cooked
  layout the engine expects, only same layout replacements make sense there.
- Files are collected once at startup, adding a file needs a restart.
- A table read on a handle opened with `FILE_FLAG_OVERLAPPED`, or one whose `OVERLAPPED` carries an
  event, is left unedited. One that goes pending gets its bytes after `Hook_ReadFile` returns, and
  one that completes at once has already signalled the game, which can take the bytes or reuse the
  buffer before an edit lands. 0.9.1 reads the package through the C runtime, which asks for
  neither. If a later version does, the log says so once. If it reads the table both ways, and an
  override adds a file, the pieces disagree on where every later slot sits, so a package entry can
  go missing or appear twice.
- A table read the hooks never see reaches the game unedited, and no line says so. That covers
  DirectStorage, a mapped view and `ReadFileEx`, and also a package opened from a module loaded
  after `Install` or through `GetProcAddress`, since only the static imports of the modules loaded
  at that point are patched. `table rebuilt` prints only once some read of the table has been
  edited. If one has, the overrides in the unseen pieces quietly do not apply, with the same
  missing or doubled entry at a seam when an override adds a file.
- Diagnostics: `trace_file_io=1` logs the first 200 package reads that are not table chunks.
