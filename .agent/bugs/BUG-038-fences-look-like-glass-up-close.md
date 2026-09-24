---
name: BUG-038-fences-look-like-glass-up-close
kind: bug
description: the wire catch fences above the barriers show as flat see through grey sheets when the car is close to them, first reported by a player on a 4090 and seen on the owner's machine on 2026-09-24 with the texture pool full and the game turning away sharper levels for space
updated: 2026-09-24
links: [reported-working-configurations, BUG-001-texture-low-mip-shown-before-streaming, BUG-020-overloaded-streaming-blurs-textures-until-they-get-tiles, BUG-035-the-writing-on-the-ground-is-pixelated, BUG-036-textures-resolve-in-visible-steps-and-a-mid-lap-restart-makes-it-worse, DEC-017-streamer-reload-fix-refuses-the-drop]
area: streaming
status: open
severity: bug
reported: 2026-09-24
parent:
---

## Symptom

Owner, 2026-09-24, while playing: "the fences becoming like glass bug appeared on my machine too? ...
It only happens when im close to a fance?"

The screenshot is from the cockpit, stopped facing a tyre wall with the catch fence above it. The fence
panels read as flat grey sheets with the sky and a Ferris wheel showing through, and no wire pattern,
while the tyres, the posts and the cockpit are sharp. The same look was reported once before by a
player on a 4090, in [reported-working-configurations](../memory/reported-working-configurations.md).

## What the log showed at the time

The session's mod log, ini and game log are kept in `logs/glass-fence-20260924`, copied at 16:02 while
the game still ran. At 15:57 to 15:58 the tile pool sat at 15,190 to 15,360 of 16,384 used, 1024 MB,
the auto size for this 6 GB card. Loads turned away for space climbed from 13,859 to 19,375 in 70 s,
about 80 a second, and the reload fix's refused drops went from 94 to 132.

## Reading

A catch fence is a wire mesh drawn from a texture whose holes are transparent. A blurred level of that
texture averages the wires and the holes into an even, partly see through grey, which reads as glass.
From a distance that level is the right one, so the look would only show close up, where the game wants
the sharp level and does not have it. That fits "only when close" and a pool turning sharper levels
away. Not known yet is whether the fence's own levels were among those turned away, or whether the game
draws the fence that way by itself.

## What would settle it

The same fence from the same spot with and without the mod, the game restored by Steam's file check for
the run without it. Glass without the mod too makes it the game's own streaming under a full pool, as
BUG-001 was. Glass only with it points at the mod's streamer changes, and `streamer_reload_fix=0` comes
next, since that fix keeps pool space the game asked back, though BUG-020 found it made no difference to
the overload there. A run with `streaming_trace=1` parked at the fence would show which level the fence
texture was given.
