---
name: dstorage-dll-is-only-a-forwarder
kind: memory
description: dstorage.dll forwards to dstoragecore.dll, which holds the runtime and is loaded by bare name from the exe folder, and nothing checks that the two versions match
updated: 2026-09-12
links: [DEC-015-bundled-directstorage-core-loaded-first, DEC-007-drag-and-drop-install-with-bundled-runtime, directstorage-streaming]
type: project
area: streaming
---

`dstorage.dll` is about 200 KB and contains no runtime. It exports `DStorageGetFactory`,
`DStorageSetConfiguration`, `DStorageSetConfiguration1` and `DStorageCreateCompressionCodec`, and
each one forwards into `dstoragecore.dll`, which is 1.4 MB and is the actual runtime. So replacing
`dstorage.dll` alone, which is what the mod is, never changes the runtime version at all.

How the core gets found, read out of the 1.3.0 forwarder's disassembly:

1. `GetModuleHandleExW` for the running executable, then read a `DStorageSDKVersion` data export
   from it. Assetto Corsa EVO exports no such symbol, so no minimum version is demanded and any
   core is accepted.
2. `LoadLibraryExW(L"dstoragecore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)`, an inbox copy
   first. Windows 11 26200 has none.
3. On `ERROR_MOD_NOT_FOUND`, build the path from `GetModuleFileNameW(nullptr, ...)`, the
   executable's own folder, and load it from there. This is the one that finds the game's copy.
4. If the core cannot be loaded at all, `DStorageGetFactory` returns `E_NOTIMPL`.

Two things about this are easy to get wrong:

- **Nothing checks the forwarder against the core.** A 1.3.0 `dstorage.dll` on a 1.2.3
  `dstoragecore.dll` returns `S_OK`, creates queues and runs the old code with no warning. The
  only reliable answer is to read `DStorageSDKVersion` back off the loaded core module, which the
  mod now logs at start. The version numbering is minor and patch only, so 203 is 1.2.3, 204 is
  1.2.4, 300 is 1.3.0 and 400 is 1.4.0.
- **A module already loaded under that base name wins.** Loading our own
  `acevo_perf\dstoragecore.dll` by full path before the forwarder runs makes step 2 and step 3
  both return our handle. Measured against a copy next to the exe and against
  `LOAD_LIBRARY_SEARCH_SYSTEM32`, which is the one that matters if Windows ever ships an inbox
  runtime.

The game's own `dstoragecore.dll` is byte identical to the 1.2.3 NuGet package, same as its
`dstorage.dll`.
