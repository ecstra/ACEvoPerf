---
name: DEC-027-the-tile-pool-goes-back-to-the-table-and-the-budget-rule-is-experimental
kind: decision
description: tile_pool_mb=auto goes back to the 0.3.2 table by dedicated memory, with 0.4's small card steps, and DEC-025's budget rule moves behind [experimental] tile_pool_from_budget, off by default, after 0.4's first day brought a Proton player from 90 fps to 20 and a game that would not start, and no card above 6 GB had ever run the rule
updated: 2026-10-08
links: [DEC-025-the-tile-pool-is-sized-from-the-budget-at-launch, DEC-022-every-card-gets-a-size-and-the-pick-is-checked-after, BUG-040-big-cards-get-a-texture-pool-far-below-what-the-game-would-use, BUG-034-an-integrated-gpu-with-a-large-uma-carve-out-is-read-as-a-card-that-size, directstorage-streaming, engine-flags]
date: 2026-10-08
area: streaming
status: standing
superseded-by:
---

## Decision

`tile_pool_mb=auto` gives the table by dedicated memory again: 1024 MB under 7 GB, 1536 under 11 GB,
2048 under 15 GB and 3072 above, the 0.3.2 figures, keeping the 256 and 512 MB steps under 5 GB that
0.4 added for small cards (DEC-022). DEC-025's rule, the budget less a reserve that grows with the
display, stays in the code and runs only with `tile_pool_from_budget=1` under `[experimental]` in
`acevo_perf.ini`, off by default. Taken on the owner's word on 2026-10-08, the day 0.4 shipped.

Two reports came in within hours of 0.4. A player under Linux and Proton went from 90 to 100 fps on
0.3.2 to about 20 on 0.4 with a broken rear view mirror, on the same machine. Another said the game
would no longer start. Neither gave a card. Reading the code against DXVK's source, the DXGI that
Proton supplies reports the whole card as the budget, with none of the share Windows holds back for
itself and the desktop, so the rule's 4200 MB reserve, measured against a Windows budget of 87 percent
on the 6 GB card, leaves a 10 GB card at 1440p about 5.5 GB of tiles where 0.3.2 gave 1536. A track
and a car then overflow the card, vkd3d-proton moves allocations to system memory and render targets
fail, which fits both symptoms. The reserve also leaves 8 to 10 GB Windows cards almost no margin in a
heavy race, and a PC where the rule reads the wrong adapter, two cards or a large UMA carve out, can
get up to 6144 MB of tiles on a card that cannot hold them, where the table stopped at 3072.

Every log on disk was from the 6 GB reference card, where the rule lands on the table's 1024 MB, plus
one integrated GPU run. No card above 6 GB had run the rule before it shipped to every player.

## Alternatives

Hold back 13 percent when the reported budget equals the card, which fixes the Proton case and leaves
Windows as it was. It answers only the one cause that is understood, and the wrong adapter and thin
margin cases stay on by default for every big card.

Keep the rule and stop it going past the table on PCs with more than one adapter. Same objection, and
both still leave an unmeasured rule as the default.

The 0.3.2 table exactly, 1024 MB for every card under 7 GB. The small card steps came from a separate
fix for cards whose 1024 MB pool and mesh cap overran them, and nothing points at them.

## Consequences

Cards above 6 GB get the pre 0.4 pools again, which leaves big cards with memory unused, so BUG-040 is
open again with its fix as an opt in. A player who wants more texture memory turns the switch on or
sets a fixed `tile_pool_mb`. The rule goes back on by default only once cards above 6 GB have run it,
and its reserve needs the Proton budget taken into account before then.
