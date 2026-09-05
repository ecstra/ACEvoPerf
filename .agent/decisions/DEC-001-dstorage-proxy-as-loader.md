---
name: DEC-001-dstorage-proxy-as-loader
kind: decision
description: the mod ships as a dstorage.dll proxy next to the game exe
updated: 2026-09-05
links: [proxy-architecture, directstorage-streaming]
date: 2026-09-05
area: foundation
status: standing
superseded-by:
---

## Decision

Ship the mod as `dstorage.dll`. The game imports `DStorageGetFactory` from the DLL in its own
folder, so the loader maps the proxy before the game's entry point runs. The original Microsoft
runtime is kept beside it as `dstorage_orig.dll` and every export is forwarded to it
(`src/dllmain.cpp`, the four `extern "C"` exports).

## Alternatives

- `dxgi.dll` proxy: also loaded early, but DXGI has many more exports to forward and the
  DirectStorage factory would still need a separate hook.
- Injector or launcher process: more moving parts, breaks when the game starts from elsewhere,
  has to stay running.
- `WinPixEventRuntime.dll` or `OptickCore.dll` proxy: not in the import table, load timing not
  guaranteed.

## Consequences

- The DirectStorage factory and queues can be wrapped with no extra hooking.
- A game update that ships a new `dstorage.dll` overwrites the proxy, so `dist/install.ps1` has to
  be rerun after updates and recognises a fresh Microsoft DLL by its version info.
