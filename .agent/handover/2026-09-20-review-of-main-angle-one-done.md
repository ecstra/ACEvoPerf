---
name: handover-2026-09-20-review-of-main-angle-one-done
kind: doc
description: state of the full review of main after the first of thirteen angles closed and merged into 0.4, what the batch loop actually caught, and exactly where the next session starts
updated: 2026-09-20
links: [reviews-index, DEC-021-a-version-branch-collects-work-and-main-holds-the-release, house-rules-agent, BUG-033-a-menu-view-remade-at-a-new-size-is-not-recognised, TODO-029-stop-shipping-dstorage-orig-and-fall-back-to-the-games-own-core]
---

# Handover: the review of main, one angle of thirteen done

## Where the repository is

`main` sits at `afd3e12`, which is 0.3.2 exactly as published. It does not move again until 0.4 ships.
`0.4` is the branch everything merges into, and it is 47 commits ahead. Both are the only branches that
exist. This is DEC-021, taken on 2026-09-20: work collects on a branch named for the version being built
and main holds the release, so the newest commit on main is always the newest published zip. The version
files and the `CHANGELOG.md` heading were renamed from 0.3.3 to 0.4 on that branch, and the build stamps
0.4.0.0.

## What the review is

The project reached 0.3.2 with no code review, by the owner's own account, so this one covers the whole of
main rather than a branch. It ran on 2026-09-20 at high across fourteen parallel angles, eight over the
source and six over the release, the tooling and the agent directory. The gate passed first: `build.ps1`
exits 0 with one warning in the whole tree, a narrowing conversion instantiated from
`src/engine/flags.cpp:152`.

151 findings: 12 breaks, 48 bug, 62 debt, 29 nit. They are split into thirteen branches, one per angle,
each with its own ledger under `.agent/reviews/2026-09-*/ledger.md`. The full picture, the branch order
and the reasoning behind it are in [reviews-index](../reviews/INDEX.md), which is the first thing to read.

## What is done

One angle. `fix/review-cohtml-build-guard`, seven review findings plus thirty more the hunters and
verifiers turned up, across five batches, all five runtime confirmed on the owner's machine, merged into
0.4 and deleted.

What it fixed, shortest form: the mod called Coherent Gameface through hardcoded vtable slot numbers with
no check that the loaded engine was the build those numbers came from, while every byte patch in the
project checked and stood down. It now checks the engine's stamp and image size and hooks nothing at all
on any other build. Then the menu view identification, the page fixes script's error paths, the resource
work move's stop flag, and what the script costs on pages where it cannot apply.

## What the loop actually caught, which is the point

Across those five batches the hunter and verifier sub agents found twenty one defects in fixes that had
already been written, built and reasoned about. Every batch's first attempt needed correcting. The ones
worth remembering:

- A bounded wait added to prevent a hang would have frozen the frame thread for ten seconds at every exit,
  because it never discounted the call it gave up on and shutdown calls the stop twice.
- A trade described in a comment as "risks tearing Cohtml down under its own worker" was in fact a use
  after free, unhandled, on a thread the game knows nothing about.
- A gate written to keep a per frame callback off the driving page removed nothing, because it gated on
  something true of every page the mod has ever seen. The evidence disproving it was already in `logs/`.
- A comment recording why a counter had to be at the top level was deleted in the same commit that moved
  it, which reintroduced a finding a previous batch had fixed and measured.

The batch loop is not ceremony here. It caught things that were built, reviewed and believed.

## What is next

Angle two, `sweep/review-render`, branched from `0.4`. Its ledger is
`.agent/reviews/2026-09-sweep-review-render/ledger.md`, twelve findings, four batches, the first of them
the one that matters: `AutoTilePoolMb` has no bracket below 7 GB, so an integrated GPU reporting a small
UMA carve out is handed the 6 GB card's 1 GB tile pool, which is the video memory exhaustion of BUG-003,
BUG-004 and BUG-005 put back on the machines least able to take it. The shipped ini has
`tile_pool_mb=auto`, so it runs for every player.

Then the remaining eleven in the order the reviews index records. The order is severity first, with the
public docs and the agent directory branches last because their content follows what the code branches
decide.

## How the loop runs

Per `house-rules.md`, and the owner confirmed on 2026-09-20 that the hunter and verifier apply to every
batch until all the ledgers are done.

1. The owner acks a batch. Nothing starts without that.
2. Fix it, one commit per fix, and move the finding's `status` and `fix` fields in the same commit.
3. A hunter sub agent sweeps the batch's scope and attacks the fix itself. Its finds enter the ledger as
   `found-by: hunter` and are fixed in the same batch.
4. A separate verifier checks bug wise, not code wise. Loop until it comes back clean.
5. Sub agents run one at a time.
6. The runtime gate is the owner's launch. They launch, never the agent.

## Things that will bite the next session

- **Install before asking for a run.** One batch was verified against a stale install because the build
  went to `dist/` and was never copied in. The log string it renamed is what caught it. Check the
  installed binary carries the change before asking for a launch.
- **`ACEVO_GAME_DIR` is set at user scope now**, but a fresh PowerShell process does not always inherit
  it. Set `$env:ACEVO_GAME_DIR` from the user scope value in the same command as `build.ps1 -Install`.
- **UI verification needs the probe.** The page fix counters only reach a log through
  `[developer] ui_probe=1`, and those lines land in the game's own log, not `acevo_perf.log`. Turn it back
  off afterwards.
- **Indexes are checked by counting files**, never by reading the previous line. Six upkeep misses on the
  first branch came from treating the paper as cleanup after the code.
- **Log a path before investing in it.** Batch 4 spent two hunter and verifier rounds and five commits on
  a stop path that a single added log line then proved never runs at all.

## Open, and deliberately so

- `H-23` in the closed ledger: Cohtml vtable slots 2 and 3 have no recorded provenance and nothing has
  ever been seen calling them. A drain log now makes it falsifiable. Settling it needs the disassembly.
- [BUG-033](../bugs/BUG-033-a-menu-view-remade-at-a-new-size-is-not-recognised.md), a menu view remade at
  a different size is not recognised. Both signals that would close it were declined with reasons.
- `F-07` of the closed ledger, deferred to `fix/review-shutdown`, which owns the detach path and the same
  hazard in four other files.
- [TODO-029](../todos/TODO-029-stop-shipping-dstorage-orig-and-fall-back-to-the-games-own-core.md), drop
  the forwarder from the zip and call the game's own core directly. Owner deferred it until the review is
  finished.
