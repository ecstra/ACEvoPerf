---
name: TODO-030-try-to-reproduce-the-crash-joining-a-session-with-a-custom-track
kind: todo
description: load a custom track with the mod on to reproduce a player's report of a crash joining a session with custom track mods installed under 0.3.2, the likeliest lead the mod's 128 to 256 MB loading buffer failing a bigger request a custom track makes
updated: 2026-09-24
links: [DEC-003-staging-buffer-128mb, BUG-003-menu-icons-stop-rendering, BUG-004-crash-on-car-or-track-change, BUG-005-crash-on-startup, directstorage-streaming, session-leak-fix, package-override-layer]
status: open
by: owner
area: stability
born: 2026-09-24
done:
---

## What

A thing to try and reproduce, in the owner's words:

> we will try and replicate it (just load a custom map). This one may be reproduceable.

The player's report from reddit, as the owner passed it on:

> Hey! I came back cause I have issue so when you have orginal ace files i have still issues that i had but when i use custom track mods i can load session with orginal ace files but if i use aceperf 0.3.2.0 it crashes when joining a session

Read plainly: with custom track mods installed, the game loads a session without the mod and
crashes joining one with ACEvoPerf 0.3.2. Filed first as BUG-037 on 2026-09-24 and moved here the
same day, since nothing is reproduced yet.

## Why

A crash on joining a session is the worst thing the mod can do to a player, and custom tracks are
content the mod has never been run against. The leads, in the order they are worth testing:

1. **The loading buffer.** The mod sets DirectStorage's staging buffer to 128 MB on a card under
   7 GB, 192 MB under 11 GB and 256 MB above, where the game asks for 1024 MB, and the runtime fails
   outright any single request larger than the buffer (`AutoStagingMb` in `src/render/adapter.cpp`,
   the same brackets in 0.3.2). The stock game's largest request is 96.2 MB, so a custom track with
   one bigger request would load without the mod and fail with it. The cap exists for BUG-003 to
   BUG-005, see DEC-003. `staging_buffer_mb=0` under `[directstorage]` leaves the game's own size.
2. **The session leak fix**, which runs exactly when a session connects. `session_leak_fix=0` under
   `[engine]`.
3. **The texture streamer fixes.** `streamer_reload_fix`, `streamer_rank_fix` and
   `streamer_partial_loads` under `[engine]`.
4. **The package override layer**, if the track's installer repacks `content.kspkg`. 0.3.2
   recognised the package's table by one empty slot alone, fixed on 0.4 by H-14 of the override
   layer review. `enabled=0` under `[overlay]`.

## Done when

A custom track has been loaded and a session joined with the mod on, noting how the track installs,
since an installer that repacks `content.kspkg` changes what the override layer reads.

- If it crashes, the mod's `acevo_perf.log` and the game's own log of that launch are kept, the
  leads above are tried one setting at a time, starting with `staging_buffer_mb=0`, and the defect
  moves to bugs/ with that evidence.
- If it loads clean, the leads go back to the player with a request for their two logs.
