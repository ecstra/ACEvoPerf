---
name: DEC-013-overtake-front-door-github-mirror
kind: decision
description: the Overtake listing is the mod's front door and the GitHub release carries the same zip as a mirror, both stay, because Overtake needs a login to download and filtered the page once
updated: 2026-09-06
links: [build-and-release, DEC-007-drag-and-drop-install-with-bundled-runtime]
date: 2026-09-06
area: release
status: standing
superseded-by:
---

## Decision

The mod is published in two places that carry the same zip. The Overtake listing
(`overtake.gg/downloads/acevoperf.86467`) is the front door: the description, the discussion,
the reviews, the download counter people look at. The GitHub repository is the mirror: the
source, the changelog, and every version as a release with the zip attached. The readme, the
repo's About link and the release notes point at Overtake, Overtake's information link points
at the repo. The owner's word on 2026-09-06: "keep it as is".

## Alternatives

- Overtake only, GitHub for the changelog: proposed by the owner and turned down on the day.
  Overtake asks for an account before a download, the reddit posts already link the GitHub
  release, and a filter took the listing down for an afternoon the same day it went up, with
  no notice. The mirror costs one command per version.
- GitHub only: the listing is where the game's players look, and it brought a review and ten
  downloads in its first hours.

## Consequences

- Cutting a version means both: `release.ps1`, the GitHub release with notes in the readme's
  voice, then "Post an update" on the listing with the same zip and version. The
  build-and-release doc carries the steps.
- The listing text and the readme say the same things in the owner's voice, the readme in
  the house format with the banner and badges, the listing in plain paragraphs. A change to
  what the mod does updates both.
- A moderation problem on Overtake never leaves people without a download.
