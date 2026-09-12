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
- **A module's base name is its identity, and the game claims it first.** Whoever loads a
  `dstoragecore.dll` first owns that name, and every later `LoadLibrary` of any path gets the same
  module back, silently and with no error. `AssettoCorsaEVO.exe` holds a UTF-16
  `dstoragecore.dll` string and loads its own copy during start-up, before it calls any export of
  ours, so no amount of preloading by the proxy can win the name. Trying it cost a build, an
  install and a launch, and only the version read back off the loaded module showed it had failed.

The way around it is not to want the name. The mod ships its runtime as
`acevo_dstoragecore.dll` and calls `DStorageGetFactoryCore`, `DStorageSetConfigurationCore` and
`DStorageCreateCompressionCodecCore` on it directly. That is exactly what the forwarder does: each
of its exports resolves the matching `...Core` symbol and tail jumps to it with the arguments
untouched, and only plain `DStorageSetConfiguration` does any work, copying the seven older fields
into the eight field struct and zeroing `ForceFileBuffering`. The core imports WINMM, dxgi, ntdll,
kernel32, d3d12 and oleaut32, and nothing from the forwarder, so it stands alone. Two runtimes
resident in one process is fine, a loaded module does nothing until an entry point is called.

The game's own `dstoragecore.dll` is byte identical to the 1.2.3 NuGet package, same as its
`dstorage.dll`.
