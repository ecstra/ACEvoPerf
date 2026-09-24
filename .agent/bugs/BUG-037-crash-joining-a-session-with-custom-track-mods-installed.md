---
name: BUG-037-crash-joining-a-session-with-custom-track-mods-installed
kind: bug
description: a player on reddit reports that with custom track mods installed the game crashes when joining a session under ACEvoPerf 0.3.2 and loads with the stock files, not reproduced, the likeliest lead the mod's 128 to 256 MB loading buffer failing a single custom track request that the game's own 1024 MB buffer would hold
updated: 2026-09-24
links: [BUG-003-menu-icons-stop-rendering, BUG-004-crash-on-car-or-track-change, BUG-005-crash-on-startup, directstorage-streaming, session-leak-fix, package-override-layer]
status: open
severity: breaks
area: stability
reported: 2026-09-24
parent:
---

## Problem

A player wrote on reddit, and the owner passed it on on 2026-09-24:

> Hey! I came back cause I have issue so when you have orginal ace files i have still issues that i had but when i use custom track mods i can load session with orginal ace files but if i use aceperf 0.3.2.0 it crashes when joining a session

Read plainly: with custom track mods installed, the game loads a session with its original files,
meaning without the mod, though the player still has the problems that brought them to the mod.
With ACEvoPerf 0.3.2 installed, the game crashes when joining a session.

Not reproduced. The owner's install has no custom track mods.

## Evidence

The report only. No log from the mod or the game, and no card, track or install method named.

## Leads

In the order they are worth testing. Each is a guess until a log says otherwise.

1. **The loading buffer.** The mod sets DirectStorage's staging buffer to 128 MB on a card under
   7 GB, 192 MB under 11 GB and 256 MB above, where the game asks for 1024 MB, and the runtime fails
   outright any single request larger than the buffer (`AutoStagingMb` in `src/render/adapter.cpp`,
   the same brackets in 0.3.2). The stock game's largest request is 96.2 MB. A custom track with one
   bigger request, a large terrain texture for instance, would load without the mod and fail with
   it, and a track loads exactly when a session is joined. The cap exists because the game's own
   1024 MB buffers starved small cards (BUG-003 to BUG-005), so the fix is unlikely to be dropping
   it. `staging_buffer_mb=0` leaves the game's own size and tests this directly.
2. **The session leak fix.** It runs exactly when a session connects and frees the session before
   last (`src/engine/session_leak_fix.cpp`). A custom track's load path could differ from the ones
   it has run on. `session_leak_fix=0` under `[engine]` tests it.
3. **The texture streamer fixes.** A custom track's textures can be laid out unlike the stock ones.
   `streamer_reload_fix`, `streamer_rank_fix` and `streamer_partial_loads` under `[engine]` test
   them, together first and one at a time after.
4. **The package override layer.** Custom track installers can repack `content.kspkg`, and 0.3.2
   recognised the package's table by nothing more than one empty slot, which a repacked package with
   another layout could pass (H-14 of the override layer review, fixed on 0.4). `enabled=0` under
   `[overlay]` tests it.

## What would settle it

- The player's `acevo_perf.log` from a launch that crashed, and the game's own log of that launch
  from `Saved Games\ACE\Logs`, whose crash lines name the module and the stack.
- Which custom tracks, how they were installed, and the card with its video memory.
- The same launch with `staging_buffer_mb=0` under `[directstorage]`, the likeliest lead. If it
  still crashes, the other switches above, one at a time.
