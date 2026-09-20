---
name: TODO-020-cut-0-3-2-after-the-three-open-investigations
kind: todo
description: 0.3.2 is not cut until the streaming layer, the 1 percent lows and the VRAM overhead have each had their round, because the release is meant to be final rather than a staging post
updated: 2026-09-20
links: [public-docs, TODO-018-look-properly-at-the-streaming-layer, TODO-019-tile-upload-dedupe-done-properly, BUG-009-one-percent-lows-far-below-average, BUG-016-vram-overhead-grows-across-scene-loads, build-and-release, one-percent-lows-2026-09-14, memory-creep-2026-09-14, DEC-019-ui-lag-work-reopened, BUG-014-ui-pages-lag-on-open-switch-and-interaction]
status: open
by: owner
area: release
born: 2026-09-12
done:
---

## What

Owner wording, 2026-09-12: "dont ship yet. ship is final after 2,3,4."

The three, in the owner's numbering from that message:

1. [TODO-018](TODO-018-look-properly-at-the-streaming-layer.md), look properly at the streaming
   layer
2. [BUG-009](../bugs/BUG-009-one-percent-lows-far-below-average.md), the 1 percent lows
3. [BUG-016](../bugs/BUG-016-vram-overhead-grows-across-scene-loads.md), the VRAM overhead that
   climbs across scene loads

Each gets its round. Then 0.3.2 is cut.

[TODO-019](TODO-019-tile-upload-dedupe-done-properly.md), the dedupe, is explicitly **not** a
gate. The owner's words: "thats later, just not now (it gets its own proper full round)." It may
land before or after this release depending on how its round goes.

## Where the three stand, 2026-09-14

- TODO-018 had its round and is done, the parked churn fixed at its source (DEC-017), with BUG-020's two
  streamer fixes landing after it.
- BUG-009 had a deep dive, [one-percent-lows-2026-09-14](../docs/research/one-percent-lows-2026-09-14.md),
  with no fix shown and two runs with no build filed (TODO-025, TODO-026).
- BUG-016 had a deep dive, [memory-creep-2026-09-14](../docs/research/memory-creep-2026-09-14.md), with
  the VRAM side recorded as placement and one run left to name the RAM growth (TODO-023).

Whether a deep dive with its runs still to come counts as the round is the owner's call. On 2026-09-14
the owner picked the UI lag as the lane to work next (DEC-019).

## What is already sitting on main waiting for it

The `0.3.2 (unreleased)` section of [CHANGELOG.md](../../CHANGELOG.md) is the list, one line per
change, written in the same commit as the change. It replaced the list that sat here on 2026-09-15,
which had fallen behind the three streamer fixes.

Plus the session of 2026-09-12, which shipped no code and closed a lot: global illumination
measured and closed, `gibake_probes_per_frame` dead, `log_pso_on_creation` dead, the Agility SDK
built and removed, Tier 2 VRS built and removed, and the tile pool reshuffle found.

## Where the three stand, 2026-09-18

All three are done. TODO-018 had its round and its fixes, BUG-009 had the HUD every frame and the child
removal fix with four clean laps measured after them, and BUG-016 is fixed by the session leak fix.

The release was cut the same day. The changelog's 0.3.2 heading carries the date, `release.ps1` built
`release/ACEvoPerf-0.3.2.0.zip` with its five files, main is tagged `v0.3.2`, and 0.3.3 is open with the
version files bumped. What is left is the publishing, the GitHub release and the Overtake update, both on
the owner's word.

That last sentence describes 2026-09-18 and stopped being true on 2026-09-20. DEC-021 moved the next
version onto its own branch and renamed it 0.4, so the version files were bumped there rather than on
main, and main sits at 0.3.2 until 0.4 ships.

## Done when

The three above have each had a round and a written outcome, `release.ps1` has been run, and the
zip is on the Overtake listing.
