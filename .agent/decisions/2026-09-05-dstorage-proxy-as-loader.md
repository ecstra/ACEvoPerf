---
title: Load the mod as a dstorage.dll proxy next to the game executable
date: 2026-09-05
status: accepted
---

## Context

The mod has to run inside the game process before the engine initialises, with nothing to install
system wide and a one file uninstall.

## Decision

Ship the mod as `dstorage.dll`. The game imports `DStorageGetFactory` from the DLL in its own
folder, so the loader maps the proxy before the game's entry point runs. The original Microsoft
runtime is kept as `dstorage_orig.dll` and every export is forwarded to it.

## Alternatives

- `dxgi.dll` proxy: also loaded early, but DXGI has many more exports to forward and the
  DirectStorage factory would still need a separate hook.
- Injector process or launcher: extra moving parts, breaks when the game is started from
  elsewhere, and needs to stay running.
- `WinPixEventRuntime.dll` or `OptickCore.dll` proxy: not in the import table, load timing not
  guaranteed.

## Consequences

- The DirectStorage factory and queues can be wrapped with no extra hooking.
- A game update that ships a new `dstorage.dll` overwrites the proxy, so the installer has to be
  rerun after updates and has to recognise a fresh Microsoft DLL.
