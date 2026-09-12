---
name: BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache
kind: bug
description: two users on the Overtake listing report that at night the car's own headlights do not light trees at the Nurburgring and light nothing at Oulton Park, and that enable_pso_cache=false cures it, which is a default the mod turns on
updated: 2026-09-12
links: [engine-flags, DEC-013-overtake-front-door-github-mirror, build-and-release]
area: render
status: open
severity: high
---

## Symptom

Reported on the mod's Overtake listing on 2026-09-10 and 2026-09-11 by two users, both
reviewing 0.3.1:

- `mld35`: at night the car's headlights do not illuminate the trees at the Nurburgring, and
  at Oulton Park they illuminate neither the track nor the trees. Other players' headlights
  in multiplayer light everything correctly. Tested with the 964.
- `MaxBal`: "Regarding the tree lighting issue, set `enable_pso_cache=false`", with the note
  that turning it off costs some micro stuttering when the same car and track are reloaded,
  which is the game's own default anyway.

Neither user posted a log. The reports are independent, one names the cause, and the named
flag is one the mod turns on and the release build ships off.

## Evidence on the reference machine

The owner's own game log of 2026-09-11 (`log-260911-184948.txt`), mod 0.3.1 installed with
`enable_pso_cache=true`, game 0.9.1+release.6:

```
[rendering] [warning] PSO Cache: 20 pipeline requests never completed, re-enabling them
[rendering] [warning] PSO Cache: 47 pipeline requests never completed, re-enabling them
[rendering] [warning] PSO Cache: 34 pipeline requests never completed, re-enabling them
[rendering] [warning] PSO Cache: 10 pipeline requests never completed, re-enabling them
[rendering] [warning] PSO Cache: 29 pipeline requests never completed, re-enabling them
[rendering] [warning] PSO Cache:  2 pipeline requests never completed, re-enabling them
```

Six warnings, clustered at the menu load and at the Red Bull Ring scene load, the 47 landing
40 ms before `Track resources streaming took 4.25 s`.

Two sessions ran that evening and only the first one warned:

| session | ran | PSO warnings | cache on disk at exit |
|---|---|---|---|
| `log-260911-184948` | 18:49 to 19:17 | 6 | none written |
| `log-260911-191750` | 19:17 to 20:37 | 0 | 31.3 MB written at 20:37 |

`Saved Games\ACE\pipeline.library` carries a creation time of 20:37 on 2026-09-11, the second
second of the second session's exit, so no cache file existed while the second session ran.
The save folder was wiped the same day for a fresh account, and Steam had put 0.9.1 in place
that morning.

The cache lives outside the game folder, so it survives a game update, a mod update and a mod
uninstall. Nothing in the game log says it is versioned against the build.

## Reading

The engine asks the cache for a batch of pipeline state objects, some never answer, and the
engine re-enables those requests and compiles them itself. It is a recovery path, so the
warning on its own does not prove a wrong picture on screen.

The session that warned and the session that did not differ in whether a cache file was
present, and the shape fits: with no file the engine has nothing to ask and compiles
everything, with a file written by a different build it asks and gets nothing back for the
entries that moved. That would make the reports an update artefact rather than a fault of the
cache in steady state, and it would explain why most users see nothing. It rests on the wipe
having happened between the two sessions, which is not in the logs, so treat it as the leading
candidate and not as established.

What is established: this cache is not clean in a shipping build, Kunos ships it off, and it
is the mod's only default that fixes nothing. It buys shader compile stalls back on repeat
runs. That is a cosmetic convenience standing against a broken night scene for an unknown
share of users.

## Reproduce

1. Install 0.3.1 with the shipped ini, `enable_pso_cache=true`, and keep the
   `pipeline.library` the game has already built.
2. Drive a night session at the Nurburgring in a car with headlights, look at the trees beside
   the road. Oulton Park at night is the stronger case, the reporter saw the track itself
   unlit there.
3. Set `enable_pso_cache=false`, delete `Saved Games\ACE\pipeline.library`, repeat.

The cache has to be deleted between the two runs, otherwise the run with the flag off still
reads a file the previous run wrote.

The update reading above is settled by one cheaper run: keep the flag on, keep the cache the
game has now, and check the game log for `PSO Cache: N pipeline requests never completed` on a
launch where the exe has not changed since the cache was written. No warnings there and
warnings after the next game update would confirm it.

## Fix

`enable_pso_cache=false` in `dist/acevo_perf.ini` since 2026-09-12, with the reason and the
`pipeline.library` warning in the comment above it. The readme no longer advertises the shader
cache, the changelog carries the entry under Unreleased. The flag stays available for anyone
who wants the stalls back.

Verified once, on 2026-09-12. The owner drove a night Nurburgring single player session of six
to eight minutes with the flag off and saw nothing wrong, in his words "flashlight is working?
idk what the issue was". That session's log carries
`flag enable_pso_cache = false (bool, was false)` at both the early and the late pass, and the
game logged one `PSO Cache: N pipeline requests never completed` line for the whole 26 minutes
against six in the 28 minute session of 2026-09-11 with the flag on.

One machine, one negative result. It confirms the flag is off and the symptom did not come
back, not that the cache was the only way to produce it. Enough to ship. The two reporters on
Overtake are the ones who can actually close this.

Still to say when it ships, because the mod cannot reach the file: anyone who saw this should
delete `Saved Games\ACE\pipeline.library` once.

## The warnings do not stop with the flag off, 2026-09-12

Three short parked runs the same evening, every one with
`flag enable_pso_cache = false (bool, was false)` at both passes, and every one still logging the
warning at each scene load:

| run | warnings | counts |
|---|---|---|
| A | 4 | 5, 30, 5, 52 |
| B | 3 | 17, 30, 1 |
| C | 3 | 17, 28, 2 |

So `PSO Cache: N pipeline requests never completed, re-enabling them` fires three to four times in
a three minute session with the cache switched off. The one line in 26 minutes quoted above was
read as the flag taking that path out of play. It does not. The rate tracks scene loads rather
than the flag, and a long driving session simply has fewer loads in it than three launches do.

What this changes: the flag is still off and the owner still did not see the symptom return, so
the shipped fix stands. What it removes is the explanation. Whatever `enable_pso_cache` does, it
does not stop pipeline requests from going uncompleted, so the mechanism behind the unlit trees is
not yet understood and this bug is not really diagnosed, only avoided. See
[engine-flags-in-game-2026-09-12](../docs/research/engine-flags-in-game-2026-09-12.md).

Also noted there: `log_pso_on_creation` emits nothing at all in the release build, so the obvious
instrument for this bug does not exist.
