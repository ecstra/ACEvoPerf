---
name: package-override-layer
kind: doc
description: how loose files under acevo_mods replace or add entries of content.kspkg at run time
updated: 2026-09-05
links: [content-package, directstorage-streaming, proxy-architecture, TODO-007-package-override-layer, TODO-009-overlay-serves-copies-so-loose-files-stay-editable]
---

# Package override layer

Files under `<game>\acevo_mods\<package path>` shadow entries of `content.kspkg`. The package on
disk is never written. Code: `src/overlay/overlay.cpp`, ini section `[overlay]`.

## How the engine reads the package

- At startup the engine opens the package through the C runtime (`ucrtbase.dll`, `fopen`) and
  reads the last 64 MB, the table of contents, in 4 KB `ReadFile` calls starting at the table
  offset. It opens and closes the package a few more times right after, without reading.
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
3. Reads at a virtual offset (`Hook_ReadFile`) are served from the loose file and the file
   position is advanced as if the package had the data. Seen for no file yet, every read observed
   so far went through DirectStorage, the path exists for completeness.
4. DirectStorage requests whose offset is virtual (`OverlayRedirect`, called from
   `QueueProxy::EnqueueRequest`) get their source swapped to an `IDStorageFile` opened on the
   loose file, offset rebased. The file stays open for the life of the process, so a loose file
   that has been requested once cannot be edited while the game runs.

## Verified on 2026-09-05

A 1 KB text file of the package replaced by a marked copy, plus a new 83 byte file added next to
it: `acevo_perf.log` shows `replace ... (1029 bytes) -> virtual offset 69070749696`, `add ...
(83 bytes)`, `table rebuilt, 122398 entries used, 1 replaced` (later `2 added`), `redirected
request #1 ... +0 size 1029 -> MEMORY`, and the game log shows the markers from both files. Table
rebuild costs 80 ms at startup (64 MB read and decoded once).

## Limits

- Paths longer than 227 bytes do not fit a slot and are skipped with a log line.
- The `.texturemips` tile files are streamed by 64 KB tile, an override must keep the cooked
  layout the engine expects, only same layout replacements make sense there.
- Files are collected once at startup, adding a file needs a restart.
- Diagnostics: `trace_file_io=1` logs the first 200 package reads that are not table chunks.
