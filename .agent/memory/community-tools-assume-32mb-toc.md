---
name: community-tools-assume-32mb-toc
kind: memory
description: public kspkg tools read a 32 MB table of contents, 0.9.0 uses 64 MB
updated: 2026-09-05
links: [content-package]
type: reference
---

The public extractors for `content.kspkg` (ntpopgetdope/ace-kspkg, Nenkai/ACEvo.Package,
sa413x/kspkg-viewer on GitHub) document the table of contents as the last 0x2000000 bytes. Build
0.9.0 uses the last 0x4000000 bytes with 262,144 slots, of which 122,398 are used. The entry
layout, XOR key and FNV 1a hash in those tools still match.

It matters because a tool reading only 32 MB lands in the middle of the table on this build and
finds nothing.

Apply it by using `tools/kspkg.py`, which tries 64 MB first and falls back to 32 MB, and by
checking the slot count printed by `kspkg.py info` after a game update.
