---
name: handover-2026-09-29-review-of-main-ui-fixes-batch-1-paused
kind: doc
description: the full review of main paused at the owner's word inside the UI fixes angle's first batch, eight of thirteen angles merged into 0.4, F-01 left as a wontfix on a diagnostic run, and five open findings from its hunter and verifier to take up first
updated: 2026-09-29
links: [handover-2026-09-20-review-of-main-angle-one-done, reviews-index, review-2026-09-sweep-review-ui-fixes, responsive-ui, house-rules-agent]
---

# Handover: the review of main, paused in the UI fixes angle's first batch

## Where the repository is

`0.4` is at `b3907f2`, the merge of the telemetry angle. Eight of the thirteen angles of the full review of
main are merged into it, the Cohtml build guard, render, proxy and core, package override layer, session
leak, teardown, engine hooks and telemetry. Five are left, in order `sweep/review-ui-fixes`, then
`sweep/review-ui-probe`, `sweep/review-tools`, `sweep/review-public-docs` and `sweep/review-agent-dir`.

`sweep/review-ui-fixes` is cut from `0.4` and pushed. It holds no code change yet, only the batch 1 records.
The owner's game runs the build of `0.4` itself, and every developer switch in the owner's ini is off.

## What is verified and what is only written

Batch 1 is F-01 alone, acked by the owner on 2026-09-29. It is a wontfix, verified by one launch with a
temporary diagnostic, `logs/uifix-b1-diag-20260929`. The diagnostic was never committed and was taken out
straight after. It logged the first time each thread built a feature set, filed a rule or removed a child,
counted any removal made while a rule was being filed, and counted any rule filed into a set a removal
had already used narrowed. Both counters stayed at zero across menu pages, a load and part of an out lap.
Cohtml's style work runs on the game's five Render Worker threads and GameThread, whose ids change at
every load, and never on the mod's moved resource thread.

The batch's hunter and verifier then raised five findings, recorded open and not fixed.

- V-01 to V-03: the doc paragraph and F-01's fix line claim more than one launch shows. They should say
  "not reached in one diagnostic launch", "part of an out lap", that GameThread only built a set, and that
  the run did not count moved work, dropping "in its style work".
- H-01: the UI probe's child removal line can report a removal and its marks in different seconds. One
  64 bit atomic holding both counts is the fix, and only a launch with `ui_probe=1` can show it.
- H-02: F-04's shape in the menu refresh fix, filed under batch 3.

The hunter also found F-14, which the Cohtml build guard angle handed to this branch on 2026-09-20 and
asked to go first. It never arrived because that ledger's pointer names a path that does not exist. It is
filed under batch 2.

The owner's two stutters in the diagnostic run were not the mod. The game restarted its audio system
twice mid lap, at 21:13:54 and 21:13:56 in `game_log.txt`, each followed by a frame of about 600 ms, most
likely the owner's Bluetooth headphones reconnecting. `logs/probe-hud-b-20260915` shows the same once.

## Next step

1. Fix V-01 to V-03 in the doc and the ledger, and H-01 in code, one commit each.
2. Run the hunter and verifier on those fixes, and decide whether H-01 earns a probe launch or is recorded
   as not showable.
3. Close batch 1, then put batch 2 to the owner for an ack. It now holds F-02, F-03 and F-14.
4. Correct the pointer in `.agent/reviews/2026-09-fix-review-cohtml-build-guard/ledger.md` to this angle's
   ledger.

## Traps

- Any fault guard put around the child removal hooks has to release `g_featureSetsLock` on the way out, or
  one handled fault freezes the UI. The note sits under F-02.
- Two points the hunter could not settle from the repo stay open, the parent's slot 49 never checked
  against 0x37B600 and the custom tag name read as a pointer at +0x10.
- The owner acks every batch, launches the game, and wants a numbered script. Nothing heavy runs while the
  owner plays, and the game's exe is never disassembled.
