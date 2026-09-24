---
name: BUG-038-fences-look-like-glass-in-sunlight
kind: bug
description: the wire catch fences show as flat see through sheets that read as glass in sunlight, the gaps between the wires filling in as the car gets closer and some fences blocked solid even far off, much less in shade, first reported by a player on a 4090 and seen on the owner's machine on 2026-09-24, most likely how the fence is drawn and lit rather than texture streaming
updated: 2026-09-24
links: [reported-working-configurations, BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache, engine-flags]
area: render
status: open
severity: bug
reported: 2026-09-24
parent:
---

## Symptom

Owner, 2026-09-24, while playing: "the fences becoming like glass bug appeared on my machine too? ...
It only happens when im close to a fance?"

Then, correcting the first reading here: "This is not a texture streaming bug. The farther I go, the more
clearer the thing is. ... the lines between the fence slowly fills in with thies new texture the clser i
get to it. Infact, sometimes even farther ones are fully blockes. and in areas with no sunlight this does
not happen that significantly?"

Three screenshots.

- Stopped facing a tyre wall, the catch fence above it reads as flat grey sheets with the sky and a
  Ferris wheel showing through and no wire pattern, while the tyres, the posts and the cockpit are sharp.
- The Nordschleife in sun, a long fence on the left drawn as a solid pale band some way off.
- The same kind of fence in shade, its wires and the tents and people behind it clear.

The same look was reported once before by a player on a 4090, in
[reported-working-configurations](../memory/reported-working-configurations.md).

## Reading

Not texture streaming. A missing sharp level would look worst up close and would not block a far fence
solid, and the owner sees the reverse. The holes of the wire mesh being drawn over, and lit, fits all
three observations, the gaps filling as the fence nears, some fences solid at a distance, and the sheet
showing mostly where the sun lights it. That is how the fence is drawn, its pipeline or its lighting,
rather than which texture level it has.

The lead is `enable_pso_cache`, the one engine flag the mod turns on that changes how the game builds its
pipelines. The game ships it off. In BUG-015 two players saw trees go unlit by headlights with it on, and
trees are drawn the way fences are, a sheet with cut out holes. The other flags the mod writes are
`no_intro`, `force_canonical_pool_sizes` and `tile_pool_mb`, none of them about drawing. The game drawing
fences this way by itself is not ruled out.

The first reading here was streaming, from a full tile pool turning sharper levels away at the time,
15,190 to 15,360 of 16,384 used and 13,859 to 19,375 loads turned away for space in 70 s. The owner's
observations rule it out.

## Logs

The session's mod log, ini and game log are kept in `logs/glass-fence-20260924`, copied at 16:02 while
the game still ran. The game log has no `PSO Cache` warning.

## What would settle it

The same fence in the sun with `enable_pso_cache=false` for one session. Right with it off makes the cache
the cause, and puts BUG-015's mechanism, which was never understood, back in question. Still glass with it
off, the same spot without the mod, the game restored by Steam's file check, tells the mod from the game.
