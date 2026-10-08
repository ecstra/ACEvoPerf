---
name: DEC-024-the-pso-cache-stays-off
kind: decision
description: the mod stops turning on the game's pipeline cache, enable_pso_cache, which the game ships off, because it draws some materials wrong for players, against a measured gain of a few seconds per load and nothing while driving
updated: 2026-10-08
links: [BUG-039-the-pso-cache-draws-some-materials-wrong, BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache, engine-flags, DEC-005-fixed-pool-sizes-by-default, pso-cache-ab-2026-10-08, TODO-031-reproduce-the-pso-cache-glitches-at-medium-textures]
date: 2026-10-08
area: engine-flags
status: standing
superseded-by:
---

## Decision

`enable_pso_cache` leaves the shipped `acevo_perf.ini`, so the mod no longer writes it and the game keeps
its own default, off. The flag still works under `[flags]` for anyone who sets it. Taken on 2026-10-08 on
the owner's question of whether the cache does anything worth keeping, after the reports gathered in
BUG-039.

## Alternatives

Keeping it on with a Known issues line, which is how 0.3.2 and the first draft of 0.4 stood. It lost
because the faults are on screen for players on several cards. What the cache buys was measured the same
day on the owner's word, [pso-cache-ab-2026-10-08](../docs/research/pso-cache-ab-2026-10-08.md): about 4 to
5 s off a load once the file exists and one short stutter fewer leaving the pits, nothing for the frame
rate or the 1 percent low while driving, and about 5 s extra on the run that builds the file.

Keeping it on but starting every run with an empty cache, so nothing is carried from one run to the next.
A player's runs with the file deleted were clean, but each was a single quick race, and the game's log
shows it consulting cached blobs after start up too. The measured gain comes entirely from the file an
earlier run wrote, so an empty cache at every launch would keep the mechanism and lose the gain, and it
needs new code in the mod for a feature the game itself ships off.

Fixing the cache. Windows' graphics layer already checks a saved pipeline against what the game asks
for, which is what the game's `stale blob, recompiling` warnings are, and the wrong drawing gets past that
check. So either the driver builds wrong code from a saved pipeline that matches, which the mod cannot
touch, or the game's own records kept beside each pipeline go stale, inside the exe, where a stricter
check of the mod's own at the graphics layer might catch it. Which one cannot be told without seeing the
glitch, and the reference machine has shown it once. TODO-031 is the attempt to see it, and the way back
to the cache if a fix then works.

## Consequences

Every pipeline is compiled when a run first needs it, as in the game without the mod. The graphics
driver's own shader cache still serves repeat compiles across runs. A player who kept an ini from 0.3.2
or earlier still has `enable_pso_cache=true`, so the changelog tells them to delete the line. The
`pipeline.library` the game wrote stays on disk, and no session with the flag off has logged a cached
blob handed back, so nothing needs deleting. A player who wants the faster loads and never sees the glitches can still set the flag under
`[flags]`, which the player docs do not advertise. Turning it back on by default would need BUG-039
fixed first, which needs the glitch seen first (TODO-031).
