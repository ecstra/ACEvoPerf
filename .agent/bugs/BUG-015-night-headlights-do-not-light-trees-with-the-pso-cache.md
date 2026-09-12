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
40 ms before `Track resources streaming took 4.25 s`. The next launch of the same build, the
same evening, logged none. That session was the first run after Steam updated the game to
0.9.1 that morning, so the cache on disk had been written by 0.9.0.

The cache is `Saved Games\ACE\pipeline.library`, 32 MB. It is not inside the game folder, so
it survives a game update, a mod update and a mod uninstall, and no line in the game log says
it is versioned or invalidated against the build.

## Reading

The engine asks the cache for a batch of pipeline state objects, the cache does not answer for
some of them, and the engine re-enables those requests and compiles them itself. That is a
recovery path, so the warning alone does not prove a wrong picture. What it does prove is that
this cache is unreliable in a shipping build, which is consistent with Kunos leaving it off.
The night lighting reports are the symptom of a permutation that does not come back: a car's
own headlight pass over vegetation is exactly the kind of rarely used permutation that would
be missing while the common ones are present, and it matches the detail that other players'
headlights are fine.

Not yet proven on this machine. The flag is the mod's only default that fixes nothing, it just
saves shader compile stalls on repeat runs, so the exchange is a cosmetic convenience against
a broken night scene.

## Reproduce

1. Install 0.3.1 with the shipped ini, `enable_pso_cache=true`.
2. Drive a night session at the Nurburgring in a car with headlights, look at the trees beside
   the road. Oulton Park at night is the stronger case, the reporter saw the track itself
   unlit there.
3. Set `enable_pso_cache=false`, delete `Saved Games\ACE\pipeline.library`, repeat.

The cache has to be deleted between the two runs, otherwise the run with the flag off still
reads a file the previous run wrote.

## Fix

Turn `enable_pso_cache` off in `dist/acevo_perf.ini` and leave it as a commented opt in with
the warning, cut it as 0.3.2, and say in the listing and the release notes that anyone seeing
it should also delete `pipeline.library` because the mod cannot reach that file. Waiting for
the owner's word.
