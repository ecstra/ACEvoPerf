---
name: BUG-039-the-pso-cache-draws-some-materials-wrong
kind: bug
description: with enable_pso_cache on, which the mod turns on and the game ships off, some materials are drawn wrong, fences as glass, trees unlit or white at night, a car's own main beam gone, pit fencing and lines flashing every frame and cars glowing, on several cards and most often on the 40 series, cured in every report by turning the flag off, and one player found a run with the cache file deleted clean and every run after it not, while the game's own log shows it handing cached pipeline blobs back and recompiling the ones it sees changed
updated: 2026-10-08
links: [BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache, BUG-038-fences-look-like-glass-in-sunlight, engine-flags, engine-flags-in-game-2026-09-12, reported-working-configurations, DEC-024-the-pso-cache-stays-off, pso-cache-ab-2026-10-08, TODO-031-reproduce-the-pso-cache-glitches-at-medium-textures]
area: render
status: fixed
severity: bug
reported: 2026-09-10
parent:
---

## Problem

Small rendering faults that all look like a material drawn with the wrong pipeline, from players on the
mod's Overtake page and on the owner's machine, between 2026-09-10 and 2026-10-08.

- Wire catch fences drawn as flat see through sheets that read as glass, filling in as the car nears and
  worst in sunlight, which one player likened to the nearest shadow cascade changing the material. RTX
  4090, RTX 4060 8 GB, and the owner's RTX 3060 Laptop once (BUG-038).
- Trees unlit by the car's own headlights at night at the Nurburgring, track and trees unlit at Oulton
  Park (BUG-015), and trees bright white in night races on another player's machine.
- A car's own main beam vanishing mid drive at night on a server at the Nurburgring 24h, the dipped lights
  and other cars' beams still drawn, seen in VR, with the same replay drawn correctly later on a monitor.
- At Donington National in daylight the top of the pit fencing flashing on and off every frame, and the pit
  bay lines flashing in the mirror. RTX 4060 8 GB, with texture quality at Ultra. Two screenshots under a
  second apart show the fence's top rail lit bright yellow in one and plain grey in the other, and the
  mirror's pit box lines changing shape between them, the rest of the scene identical.
- A car glowing. Screenshots of a BMW M3 in the menu showroom with the whole body blown out to a white
  glow, then in another showroom and on track with the roof and every window solid white while the paint
  is right, on 0.9.1, from a player who said it was normal the day before. Also reported once on an RTX
  4070 Ti with no log.

Not part of this: livery textures sharpening slowly on a game installed on a hard disk, which is
streaming from a slow drive, and a per eye jitter with DLSS in VR, which the owner sees without the mod.

## Evidence

### What the reports share

Every player who tried `enable_pso_cache=false` in `acevo_perf.ini` was cured, the fence player on the
4060 among them, and the owner has given that advice in the thread since 2026-09-19. The game itself ships
the flag off. It is the one engine flag the mod turns on that changes how pipelines are built, the others
it writes are `no_intro`, `force_canonical_pool_sizes` and `tile_pool_mb`.

The cards named are a 4090, a 4060 (twice in different scenes), a 4070 Ti and the owner's 3060 Laptop,
where it showed once in weeks of play. A player on a 2070 8 GB and the reports on 20 and 30 series cards
in `reported-working-configurations` never mention it. That leans toward the 40 series without proving it,
since nobody else said which way their flag was set.

### The cache file decides it

The 4060 player tested it repeatedly, unprompted. Deleting `Saved Games\ACE\pipeline.library` gave one
clean run, and the fault came back on every run after it, until the file was deleted again. With the flag
off it never came back. So a run that compiles every pipeline itself draws correctly, and a run that
reads the cache the previous run wrote does not, at least on that card.

Their fence case reproduced on every launch with a Ferrari 296 GT3 quick race at the Nurburgring GP and
texture quality at Medium, and not in five or six launches at Ultra. Their later Donington case appeared
at Ultra too, so texture quality changes which materials are hit rather than whether anything is.

### The game's own log

The game logs `PSO Cache: stale blob for Pipeline <number> (pipeline content changed), recompiling`.
Across the saved sessions in `logs/` it appears only with the flag on. The census run of 2026-09-16 logged
219, 208 of them in its first three minutes, a few pipelines twice and one twelve minutes after the rest,
and a clean lap run the same day 67. Three launches on 2026-09-18 logged one each, for the same pipeline
number, so the number is a stable key and that pipeline's content differs from run to run. No session
with the flag off has one.

So with the cache on, the game hands compiled blobs from the file back to pipelines, checks their content
and recompiles the ones it sees changed. A blob whose difference that check does not cover would be drawn
as it was compiled, for some other state, which fits a cut out fence or a tree drawn as a solid or wrongly
lit sheet. That is a reading. The check is inside the game and nothing in its log names a pipeline it let
through.

The other warning, `PSO Cache: N pipeline requests never completed, re-enabling them`, appears with the
flag off as well (BUG-015), so it is not this.

The 4060 player's mod log of the Donington run shows nothing out of line, a tile pool of 24,576 tiles with
22,205 used, no loads turned away and the usual load hitches. The mod's log does not see pipelines.

### A cache that starts empty at every launch

Keeping the flag on and starting each run with no file is the other way out, a cache that lives for one
run and is never carried to the next. It has some support. Every run the 4060 player started with the
file deleted was clean, though each was a single quick race. Whether it stays clean through the scene
loads of a long run is not known, and the census run shows the game consulting blobs after start up as
well, in the pipelines it flagged twice and the one twelve minutes in. The flag off is the only state
seen clean over long play on every machine that tried it.

## Fix

The mod stops turning the cache on (DEC-024), on `fix/pso-cache-glitches` on 2026-10-08, `beaca33`.
`enable_pso_cache` is out of the shipped `acevo_perf.ini`, so the mod writes nothing and the game keeps
its own default, off, the state every player who tried it was cured by. The readme no longer lists the
shader cache, and the changelog tells a player who kept an older ini to delete the line. The mod cannot
fix the cache itself, since which blob goes to which pipeline is decided inside the exe. Measured the same
day, [pso-cache-ab-2026-10-08](../docs/research/pso-cache-ab-2026-10-08.md), the cache saves about 4 to 5 s
per load once its file exists and one short stutter leaving the pits, and nothing while driving.

## Verification

The fix is the state players were already cured by, so it is verified by their reports. Every player who
turned the flag off saw the glitches go, and one tested deleting the cache file run by run until the
flag off settled it. What was checked here on 2026-10-08: the shipped `acevo_perf.ini` no longer carries
the flag, nothing in `src/` writes it, the `[flags]` reader writes only the keys an ini holds, and the
flag off launch of `logs/pso-ab-20261008` logged no cached pipeline handed back.

Not verified on the reference machine, which showed the glitch once in weeks of play. TODO-031 is the
attempt to see it here, which a fix keeping the cache would need.
