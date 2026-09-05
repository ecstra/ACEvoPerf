---
name: TODO-009-overlay-serves-copies-so-loose-files-stay-editable
kind: todo
description: the override layer keeps every requested loose file open for the life of the game, so a modder cannot edit it until the game exits
updated: 2026-09-05
links: [package-override-layer, TODO-007-package-override-layer]
status: open
by: agent
area: streaming
born: 2026-09-05
done:
---

## What

`OverlayRedirect` opens each loose file once through `IDStorageFactory::OpenFile` and keeps the
`IDStorageFile` for the rest of the session. DirectStorage opens without delete or write sharing,
so replacing the file (the edit tools write a temp file and rename it over) fails with a sharing
violation while the game runs. On 2026-09-05 every probe change meant a full game restart.

Serve a copy instead: at startup copy each collected loose file into `acevo_mods\.cache\<same
path>` and open the copy. The originals stay free. The layer already collects files once at start,
so live edits were never picked up anyway, the copy only removes the lock. Alternative: open with
a `CreateFileW` that shares delete and hand the handle to DirectStorage, but `OpenFile` takes a
path, not a handle, so the copy is the simple way.

## Why

Iterating on any content override needs edit, restart, look,
without fighting a locked file. The lock also confuses anyone syncing a mod folder while playing.

## Done when

A loose file under `acevo_mods` can be overwritten while the game runs and the layer serves the
old content until the next start, the cache folder is recreated at every start and never committed.
