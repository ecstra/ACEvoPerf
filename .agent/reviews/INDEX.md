---
name: reviews-index
kind: doc
description: index of code review ledgers, active first, with the 2026-09-20 full review of main and its thirteen branches, five of them done
updated: 2026-09-24
links: [agent-index, spec-reviews, house-rules-agent]
---

# Reviews Index

Each review gets its own folder and ledger per
[spec/reviews.md](../spec/reviews.md), listed here active first, closed
below.

## The full review of main, 2026-09-20

The project ran to 0.3.2 without a code review, by the owner's own account, so this one covers the
whole of main rather than a branch. It ran at high across fourteen angles in parallel, eight over the
source and six over the release, the tooling and the agent directory. The severe findings were then
checked against the code a second time before they were written down here.

The gate passed first. `build.ps1` exits 0, and the one warning in the entire tree is a narrowing
conversion instantiated from `src/engine/flags.cpp:152`, filed as `sweep/review-engine` F-07.

151 findings: 12 breaks, 48 bug, 62 debt, 29 nit. Five angles are done as of 2026-09-24, the Cohtml
build guard, the render layer, the proxy and core, the package override layer and the session leak
fix, and the eight below them are open with every batch pending the owner's word.

Those five added 222 findings of their own, from the hunter and verifier sub agents that run on
every batch. Every one of the twenty batches so far needed at least one correction after its first
fix. Twenty of the first three angles' added findings were bugs a fix had itself caused or left
standing, and seven more code defects in the override layer's, and in the session leak fix's one fix
did more harm than its finding and was reverted. The loop is not ceremony on this codebase.

### What the review found, in one paragraph

The reverse engineering is the strong part. Every byte patch in the project checks the game's timestamp
and image size, hashes each region before it writes, computes its displacements in advance and refuses
cleanly on a build it does not recognise. The offsets hold up under a second reading, the COM layer is
complete, the export table matches Microsoft's exactly, and the package parsing is provably in bounds.

Three weaknesses run through everything instead. Values that cross a boundary from a file are used
without a check, so an ini a player edits by hand can turn a sampler into a busy loop or hand
DirectStorage a zero sized staging buffer. Several switches are wired in series with fixes their names
never mention, so turning off a measurement quietly removes the crash fix. And failures latch silently,
so one unchecked pointer or one failed hash retires a feature for the rest of the session behind a
single log line that reads like routine.

### The branches

Thirteen branches, one per angle, each cut from the latest 0.4 in this order. The owner acks each
batch, and a branch merges before the next one starts.

| order | branch | ledger | findings | worst |
|---|---|---|---|---|
| 1 | `fix/review-cohtml-build-guard` | [ledger](2026-09-fix-review-cohtml-build-guard/ledger.md) | 7 | breaks |
| 2 | `sweep/review-render` | [ledger](2026-09-sweep-review-render/ledger.md) | 12 | breaks |
| 3 | `sweep/review-proxy-core` | [ledger](2026-09-sweep-review-proxy-core/ledger.md) | 15 | breaks |
| 4 | `sweep/review-overlay` | [ledger](2026-09-sweep-review-overlay/ledger.md) | 11 | breaks |
| 5 | `fix/review-session-leak-safety` | [ledger](2026-09-fix-review-session-leak-safety/ledger.md) | 6 | breaks |
| 6 | `fix/review-shutdown` | [ledger](2026-09-fix-review-shutdown/ledger.md) | 4 | breaks |
| 7 | `sweep/review-engine` | [ledger](2026-09-sweep-review-engine/ledger.md) | 7 | bug |
| 8 | `sweep/review-telemetry` | [ledger](2026-09-sweep-review-telemetry/ledger.md) | 10 | breaks |
| 9 | `sweep/review-ui-fixes` | [ledger](2026-09-sweep-review-ui-fixes/ledger.md) | 12 | bug |
| 10 | `sweep/review-ui-probe` | [ledger](2026-09-sweep-review-ui-probe/ledger.md) | 9 | breaks |
| 11 | `sweep/review-tools` | [ledger](2026-09-sweep-review-tools/ledger.md) | 18 | breaks |
| 12 | `sweep/review-public-docs` | [ledger](2026-09-sweep-review-public-docs/ledger.md) | 16 | breaks |
| 13 | `sweep/review-agent-dir` | [ledger](2026-09-sweep-review-agent-dir/ledger.md) | 24 | bug |

The order is severity first, with two dependencies. The public docs branch runs late because several of
its settings comments have to say whatever the code branches decide, and the agent directory branch runs
last for the same reason.

Four files are named by two branches each. `src/core/log.cpp`, `src/engine/streamer.cpp`,
`src/telemetry/load_sampler.cpp` and `src/telemetry/timeline.cpp` all carry teardown findings that
belong to `fix/review-shutdown` and other findings that belong to their own surface. Because branches
run one at a time and each cuts from the latest 0.4, that is a rebase rather than a conflict, and each
ledger says which findings it owns.

### The twelve breaks

- the responsive UI calls Cohtml vtable slots with no build check while every byte patch stands down,
  on by default, `fix/review-cohtml-build-guard` F-01
- the automatic tile pool has no bracket below 7 GB, so an integrated GPU is handed 1 GB,
  `sweep/review-render` F-01
- `stats=0` silently removes the entire package override layer, `sweep/review-proxy-core` F-01
- the 64 MB package table is built lazily with no lock from a hook in every module,
  `sweep/review-overlay` F-01
- the session free can race a plain shared_ptr copy, write through a reallocated vector, and walk a
  freed game mode with no fault guard, `fix/review-session-leak-safety` F-01, F-02 and F-03
- the detach path logs through a lock a terminated thread can still own, so the game never exits,
  `fix/review-shutdown` F-01
- the memory census leaves its import hooks in every module when it cannot open its file,
  `sweep/review-telemetry` F-01
- the UI probe can fault while it holds a game thread suspended, `sweep/review-ui-probe` F-01
- `kspkg.py extract` builds its output path from the package's own strings,
  `sweep/review-tools` F-01
- the zip's uninstall steps can leave a game that will not start, `sweep/review-public-docs` F-01

### Raised with the owner and deliberately not filed

One reviewer noted a mismatch between the name the project publishes under and the identity carried by
the public commit history. That is a fact about the owner as a person, which `spec/memory.md` and
CLAUDE.md section 8.2 both keep out of this repo, so it was raised in conversation and left out of every
ledger.

## Active

- [2026-09-fix-review-shutdown](2026-09-fix-review-shutdown/ledger.md), the log lock a dead thread can
  still own when DllMain logs, found by three reviewers independently, 4 findings, 1 breaks
- [2026-09-sweep-review-engine](2026-09-sweep-review-engine/ledger.md), one null pointer that retires all
  three streamer fixes and a flag writer that can inherit the wrong storage, 7 findings
- [2026-09-sweep-review-telemetry](2026-09-sweep-review-telemetry/ledger.md), hooks left installed when
  the census cannot open its file and a sampler that picks the wrong threads, 10 findings plus 2
  from other angles' sub agents, 1 breaks
- [2026-09-sweep-review-ui-fixes](2026-09-sweep-review-ui-fixes/ledger.md), a marking window that skips
  invalidation and a row of load bearing assumptions written down nowhere, 12 findings plus 1 from
  another angle's verifier
- [2026-09-sweep-review-ui-probe](2026-09-sweep-review-ui-probe/ledger.md), an instrument that can fault
  while holding a game thread suspended and pays its cost in the frames it explains, 9 findings, 1 breaks
- [2026-09-sweep-review-tools](2026-09-sweep-review-tools/ledger.md), a package extract that can write
  outside its output folder and parsers that produce a wrong file at exit 0, 18 findings plus 1 from
  another angle's hunter, 1 breaks
- [2026-09-sweep-review-public-docs](2026-09-sweep-review-public-docs/ledger.md), an uninstall that can
  leave the game unable to start and settings whose comments hide what they turn off, 16 findings, 1 breaks
- [2026-09-sweep-review-agent-dir](2026-09-sweep-review-agent-dir/ledger.md), knowledge docs describing
  code that changed underneath them and tracker files that break their own specs, 24 findings

## Closed

- [2026-09-fix-review-session-leak-safety](2026-09-fix-review-session-leak-safety/ledger.md), the hand
  rolled shared_ptr surgery and what it does not guard against, 6 findings plus 51 from the hunters and
  verifiers, 57 in all and 3 of them breaks, three batches, the last leaving the code as the second's
  run confirmed it, the copy and push races left open on the owner's choice in DEC-023, merged into
  0.4 on 2026-09-24
- [2026-09-sweep-review-overlay](2026-09-sweep-review-overlay/ledger.md), an unlocked lazy build of a
  64 MB table and a redirect that fails silently, 11 findings plus 58 from the hunters and verifiers,
  69 in all and 1 of them breaks, three batches, all runtime confirmed, merged into 0.4 on
  2026-09-24
- [2026-09-sweep-review-proxy-core](2026-09-sweep-review-proxy-core/ledger.md), an off switch that takes
  the override layer with it and a row of ini values used without validation, 15 findings plus 43
  from the hunters and verifiers, 58 in all and 1 of them breaks, five batches, runtime confirmed
  where a run can show it, merged into 0.4 on 2026-09-23
- [2026-09-sweep-review-render](2026-09-sweep-review-render/ledger.md), the auto sizes landing on the
  wrong card and three settings wired in series, 12 findings plus 40 from the hunters and verifiers,
  52 in all and 3 of them breaks, four batches, all runtime confirmed, merged into 0.4 on 2026-09-20
- [2026-09-fix-review-cohtml-build-guard](2026-09-fix-review-cohtml-build-guard/ledger.md), the Cohtml
  vtable calls that ignore the build check every byte patch honours, 7 findings plus 30 from the
  hunters and verifiers, five batches, all runtime confirmed, merged into 0.4 on 2026-09-20
- [2026-09-fix-streamer-feedback-churn](2026-09-fix-streamer-feedback-churn/ledger.md), the texture
  streamer reload fix, two findings about state keyed by a reused address, both fixed
