---
name: TODO-032-adapt-the-mod-to-game-version-0-10
kind: todo
description: when the game's 0.10 arrives with free roam, check every part of the mod against the new build, since every byte patch and vtable read is tied to 0.9.1's stamp and will stand down on 0.10 until it is read again, and see which UI fixes the developers' own work may have made redundant
updated: 2026-10-08
links: [responsive-ui, directstorage-streaming, session-leak-fix, engine-flags, proxy-architecture]
status: open
by: owner
area: foundation
born: 2026-10-08
done:
---

## What

The game's next update, 0.10, brings a free roam mode and changes of its own. The owner said on
2026-10-08 that the mod will have to adapt when it arrives. The game's developers have acknowledged the
mod, said they are looking into what it raises, and that it helped them with the UI.

Every part that patches the game or Cohtml checks the module's stamp, size and the bytes it changes, so
on a new build it refuses cleanly and the mod falls back to passing DirectStorage through. That keeps 0.10
safe on day one and also means most fixes are off until each is read again against the new build:

- the texture streamer hooks and their three fixes
- the session leak fix
- the responsive UI's patches and the shared Cohtml hooks
- the engine flags, which are found by scanning rather than by address
- the package override layer and the asset fixes it generates

Free roam is a new kind of session the session leak fix and the streamer have never seen.

## Done when

Each part above has been launched on 0.10 and either reads the new build and works, with the log showing
it, or stays off with the reason written in its doc. A UI fix the game's own update made redundant is
taken out rather than left patching.
