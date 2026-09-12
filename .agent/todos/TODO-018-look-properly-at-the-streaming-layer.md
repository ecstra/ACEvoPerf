---
name: TODO-018-look-properly-at-the-streaming-layer
kind: todo
description: a 22 MB/s re-read loop sat in the streaming layer since the mod began and nothing found it until a variable rate shading experiment tripped over it, so the layer this mod is named after has never actually been looked at directly
updated: 2026-09-12
links: [tile-pool-reshuffle-2026-09-12, directstorage-streaming, TODO-013-faster-session-loads, BUG-001-texture-low-mip-shown-before-streaming, BUG-007-blurry-road-and-textures, BUG-016-vram-overhead-grows-across-scene-loads]
status: open
by: owner
area: streaming
born: 2026-09-12
done:
---

## What

Owner wording, 2026-09-12: "we do need to check more on streaming since this was here and we
never knew about it till now at all?"

He is right, and the way it was found is the argument. A stationary car with a locked camera was
pulling 22 MB/s of texture tiles in a permanent loop, and it surfaced only because a Tier 2
variable rate shading experiment accidentally suppressed it and showed up as an unexplained 3
percent. Nothing in this project had ever looked.

## Why it matters more than the thing it was found chasing

This mod is a streaming mod. It caps the staging buffer, forces the tile pool, proxies every
DirectStorage queue and ships a package override layer. The streaming layer is its subject, and
the instruments pointed at it have all been **aggregate**: bytes per second, requests per second,
queue totals. Nobody asked what the individual requests were, which is why a loop re-reading the
same 28 regions forever was invisible for the life of the project.

The one time a real question was asked of it, in one evening, it produced:

- a 22 MB/s permanent re-read loop while parked, 97 percent of parked tile bytes
- about 4 percent of tile bytes redundant while driving
- 68,051 tiles mapped against **21** unmapped in a whole session, so the engine essentially never
  releases a tile
- the pool running **92 to 98 percent full**, a number BUG-016 has wanted since it was opened
- confirmation that the engine's own streamer re-requests tiles it already holds

None of that came from the mod's existing telemetry, and all of it was reachable.

## Where to point it

Not a plan yet, a list of questions nobody has asked, roughly in order of how cheap they are:

- **Does the loop exist on other tracks and in the menu**, or is it Nürburgring content. Every
  measurement so far is one track, one spot.
- **What does the loop look like during a load**, where three quarters of a session load already
  goes and where TODO-013 stalled. The request level instrument now exists and has never been
  pointed at a load.
- **Which textures dominate real driving traffic.** The offset to package entry mapping works and
  named the parked set in one pass. The same question of a lap has never been asked.
- **Is the reshuffle what drives BUG-016.** The overhead climbs across scene loads, and an
  allocator that relocates its whole resident set every two seconds is the right shape for
  fragmentation.
- **What the 21 unmaps mean.** An engine that maps 68,051 tiles and releases 21 is either leaking
  or expressing eviction some other way, and neither has been checked.

## What already exists to do it with

Built on 2026-09-12 and deleted with its branch, but recorded and cheap to rebuild:

- `log_requests=1` plus an offset to package entry mapper, which names what is being fetched
- a hook on `ID3D12CommandQueue::UpdateTileMappings` and `CopyTileMappings`, slots 8 and 9
- a shadow map of the tile pool that follows every mapping the engine makes
- the parked protocol, which is what makes any of it comparable

## Done when

The streaming layer has been examined directly rather than in aggregate, on more than one track
and during a load as well as while driving, and whatever that turns up is either a fix, a
tracker entry or a written reason it is not ours.
