---
name: DEC-017-streamer-reload-fix-refuses-the-drop
kind: decision
description: the texture streamer's flip is fixed by refusing the drop of a texture recognised as flipping from its own feedback readings, guarded only by the engine's 1024 tile margin, rather than by correcting the conversion, refusing the reload, deduplicating uploads or standing down whenever the pool is contended
updated: 2026-09-13
links: [texture-streamer-flip-2026-09-13, TODO-018-look-properly-at-the-streaming-layer, TODO-019-tile-upload-dedupe-done-properly, tile-pool-reshuffle-2026-09-12, DEC-016-agility-sdk-tried-and-removed, BUG-020-track-textures-blur-when-many-cars-are-close]
date: 2026-09-13
area: streaming
status: standing
superseded-by:
---

## Decision

The texture streamer reads GPU feedback measured against the loaded mip as if it were measured
against the full texture, so textures on the budget edge load and drop the same mip every two
kicks ([texture-streamer-flip-2026-09-13](../docs/research/texture-streamer-flip-2026-09-13.md)).
The mod fixes it by refusing the drop, and only for a texture whose readings show that exact flip,
a drop on a fresh reading and then a reload on a fresh reading no higher than the first shifted
down by the levels lost. The pin lets go when a drop leaves the flip, the reading goes stale, the reading
says the view moved away, screen coverage moves by more than a quarter, or the texture is not
admitted at all. A drop is let through without forgetting the flip whenever used plus pending tiles
no longer leave the engine's 1024 tile margin free. It ships as `[engine] streamer_reload_fix` in
`src/engine/streamer.cpp`, on by default since the owner's call of 2026-09-13, "Streamer fix keep
it", once a write detector showed nothing in the game writes into streamed textures at runtime.
`streamer_reload_fix=0` turns it off.

## Alternatives

- **Correct the conversion in the kick.** The shader clamps its reading at zero, so at a coarse mip
  zero means "this view or finer". Converting it properly would stop the upward probe that lets a
  texture sharpen at all. Rejected before it was built.
- **Refuse the reload instead of the drop.** It frees the pool for other waiting loads, but the
  texture would sit on the coarser of the two mips the screen shows today, a picture change the
  owner's rule does not allow. Holding the finer mip costs no video memory, the tile pool is a
  fixed heap.
- **Stand down whenever gate space is short or a load was turned away.** The first version.
  Boot 1 showed the pool full in normal play with about 120 loads turned away on every parked kick,
  so it would have acted 28 times in a session. The flipping texture holds its finer mip half the
  time today anyway, so contention is not what makes a refusal unsafe. The margin is.
- **A tile upload dedupe, TODO-019.** It hides the re-upload while the streamer keeps dropping,
  re-requesting and remapping, and it owns pool placement, which carried two wrong texture defects
  on 2026-09-12. Fixing the cause upstream removed the parked churn with no placement at all.
- **A priority bonus for resident levels.** No state needed, but the size of the swing it has to
  beat was never measured and it would bias every kick while driving.

## Consequences

- Five rel32 sites in the exe are rewritten, pinned by hashing every region the hooks rely on, so
  any game update leaves the fix unapplied and says so in the log. Each new build needs the regions
  re-derived before the fix works again.
- Parked, the churn is gone, measured in boot 2. While driving the fix reaches a fifth to a quarter
  of the reload traffic by replay, because coverage keeps changing. Widening that is follow up work,
  not a reason to loosen the release rules blind.
- The streaming trace, `[log] streaming_trace`, is the instrument for any further change, and the
  replay script approach (run the rule over a recorded session before a boot) is how a change gets
  checked before it costs the owner a session.
