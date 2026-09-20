---
name: DEC-021-a-version-branch-collects-work-and-main-holds-the-release
kind: decision
description: work merges into a long lived branch named for the version it is building, and main only moves when that version ships, so main and the published release never drift apart
updated: 2026-09-20
links: [house-rules-agent, build-and-release, DEC-013-overtake-front-door-github-mirror, reviews-index]
date: 2026-09-20
area: release
status: standing
superseded-by:
---

## Decision

Work branches merge into a long lived branch named for the version being built, `0.4` today. Main moves
only when that version is cut and published, by merging the version branch into it. Everything else about
the branch contract is unchanged: one branch one intent, gates then review then merge, the owner calls the
timing.

The next version is 0.4 rather than 0.3.3. The full review of main opened 151 findings across thirteen
branches, and the owner plans feature work on top of that, which is not a patch release.

The immediate reason is that main had stopped meaning anything. The 0.3.2 tag is the published zip, and
main had already moved past it with the version files bumped for the next release, so a reader cloning
main got something that was neither what players are running nor a finished version. The review is about
to put a few hundred commits on top of that, none of which anyone can download. Keeping main at the last
release and collecting the work on `0.4` makes main answer one question honestly: what is in the hands of
players right now.

## Alternatives

**Carry on merging into main.** What the project did to 0.3.2. It works while releases are frequent and
small, and it fails exactly here, where a long review lands hundreds of changes that ship together much
later. Main would spend weeks describing software nobody has.

**Cut 0.3.3 as a small release with the first few batches in it.** Rejected by the owner, in their words
"we will not release 0.3.3 just because of a few minor improvements, we can do better". The first three
batches fixed things that were silent, preventative or both, and none of them is worth asking players to
reinstall for.

**Tag branches instead of keeping a version branch.** Tags mark what happened and do not give the work a
place to accumulate. The merges still need a target, and that target is what this decision names.

**A develop branch that never changes name.** The usual shape, and it hides which release the work is for.
Naming the branch after the version means a glance at the branch list says what is being built, and the
branch dies when the version ships rather than living forever.

## Consequences

Main is released code, so the newest commit on main matches the newest published zip. A tag still marks
each release, and the merge into main is what the tag lands on.

Every branch cuts from the open version branch and merges back into it, not into main. That includes the
review branches already in flight, which were cut before this decision and merge into `0.4` because it was
created from the same commit they came from.

The version files are bumped on the version branch when it opens rather than on main after a release, so
main no longer carries a version that does not exist. `CHANGELOG.md` keeps its unreleased heading on the
version branch for the same reason.

Releases get less frequent and bigger, which is the point, and it costs players the small fixes in the
meantime. The mitigation is that anyone who wants them can build the version branch, and nothing about the
drag and drop install changes.

One thing this forecloses: a hotfix to a shipped release can no longer be a quick commit to main. It needs
its own branch from main and a merge forward into the open version branch, which is more ceremony than the
project has needed so far and is the right trade for main meaning something.
