---
name: feedback-public-docs-are-for-players
kind: memory
description: the owner called the readme and the changelog AI slop on 2026-09-15, the public files are written for players in short plain lines and checked against public-docs before any change
updated: 2026-09-15
links: [public-docs, build-and-release, feedback-ship-as-installable-mod]
type: feedback
---

On 2026-09-15 the owner asked why `README.md` and `CHANGELOG.md` read as "AI slop and wordings and
word salads". The complaints were a paragraph of mechanism behind every fix, a "what else it does"
section nobody needed, text written as if the reader knew the codebase, a build section that was one
paragraph instead of steps, and a version badge listing two builds where `0.9+` says it. Both files
and `dist/README.txt` were rewritten that day, with everything since 0.3.1 filed under 0.3.2.

It matters because these are the only files most people ever read, and every changelog entry had
been written in the voice of the bug file it came from.

Apply it by reading [public-docs](../docs/ops/public-docs.md) before touching any of the three, and by
writing a change's changelog line for the player in the same commit, with the mechanism kept in the
bug, the decision or the system doc.
