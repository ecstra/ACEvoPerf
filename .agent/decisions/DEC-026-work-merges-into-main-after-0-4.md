---
name: DEC-026-work-merges-into-main-after-0-4
kind: decision
description: after 0.4 was cut, work branches cut from main and merge back into it on the owner's word, and no version branch opens and nothing is bumped until the owner names the next version, replacing the version branch of DEC-021
updated: 2026-10-08
links: [DEC-021-a-version-branch-collects-work-and-main-holds-the-release, house-rules-agent, build-and-release, public-docs]
date: 2026-10-08
area: release
status: standing
superseded-by:
---

## Decision

Work branches cut from main and merge back into main on the owner's word. No version branch is open, and
the version files and the changelog heading stay at the last release, 0.4, until the owner names the next
version. A user visible change still gets its changelog line in the same commit, under `## Next
(unreleased)`, which takes the version's number once it is named. A release is cut from main and tagged
there.

Taken on 2026-10-08, right after 0.4 was cut, when the release steps opened `0.4.1` with the version files
bumped and the owner said "No 0.4.1, 0.4.0 still or main".

## Alternatives

**Open the next patch version at once, as DEC-021 and the release steps said.** That is what put `0.4.1`
on the remote, and it guessed the next version for the owner. The next release may well be the one that
adapts the mod to the game's 0.10, which the owner may number differently.

**Keep a version branch named `0.4.0`.** It would hold the same commits as main under the name of a release
that already shipped, which says less than main does.

## Consequences

Main no longer means "only what players have", the reason DEC-021 gave for the version branch. The tag
`v0.4` and the GitHub release still do. DEC-021 is superseded. When the owner names the next version,
the bump is one commit on a branch merged into main.
