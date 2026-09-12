---
name: decisions-index
kind: doc
description: index of every decision taken, newest first
updated: 2026-09-12
links: [agent-index, spec-decisions]
---

# Decisions Index

- [DEC-015-bundled-directstorage-core-loaded-first](DEC-015-bundled-directstorage-core-loaded-first.md), 2026-09-12, the mod ships DirectStorage 1.3.0 as acevo_dstoragecore.dll and calls the core directly, because the game owns the name dstoragecore.dll and loads its own first
- [DEC-014-reflex-ships-on-boost-ships-off](DEC-014-reflex-ships-on-boost-ships-off.md), 2026-09-12, Reflex ships on because the game has none and it costs nothing measurable, boost ships off because a thermally capped card has no clocks for it to hold up
- [DEC-013-overtake-front-door-github-mirror](DEC-013-overtake-front-door-github-mirror.md), 2026-09-06, the Overtake listing is the front door and the GitHub release the mirror, the same zip in both
- [DEC-012-no-ghost-car-mod](DEC-012-no-ghost-car-mod.md), 2026-09-06, the ghost car flag does not ship and its branch is gone, the game records and loads a ghost but nothing shows it
- [DEC-011-no-free-roam-mod](DEC-011-no-free-roam-mod.md), 2026-09-06, the Free Roam unlock does not ship and its branch is gone, the package stops at the Nürburgring complex, the recipe stays in the research record
- [DEC-010-no-ui-changes-ship](DEC-010-no-ui-changes-ship.md), 2026-09-06, no UI change ships after the lag hunt, the Cohtml hook stays as a diagnostic, the overhaul is the owner's call
- [DEC-009-pool-and-staging-sizes-by-card](DEC-009-pool-and-staging-sizes-by-card.md), 2026-09-06, tile pool and staging buffer picked from the card's memory at the game's first DXGI factory
- [DEC-008-frame-latency-left-to-the-game](DEC-008-frame-latency-left-to-the-game.md), 2026-09-05, the game's swap chain latency stays, the proxy logs the calls
- [DEC-007-drag-and-drop-install-with-bundled-runtime](DEC-007-drag-and-drop-install-with-bundled-runtime.md), 2026-09-05, zip bundles the Microsoft runtime, no install scripts
- [DEC-006-frame-latency-cap-default](DEC-006-frame-latency-cap-default.md), 2026-09-05, superseded by DEC-008, the cap never took effect
- [DEC-005-fixed-pool-sizes-by-default](DEC-005-fixed-pool-sizes-by-default.md), 2026-09-05, fixed tile pool of 1024 MB and canonical mesh cap by default
- [DEC-004-canonical-pools-off-by-default](DEC-004-canonical-pools-off-by-default.md), 2026-09-05, superseded by DEC-005
- [DEC-003-staging-buffer-128mb](DEC-003-staging-buffer-128mb.md), 2026-09-05, cap the staging buffer at 128 MB
- [DEC-002-flags-by-memory-write](DEC-002-flags-by-memory-write.md), 2026-09-05, write FLAGS_ storage found by a code scan
- [DEC-001-dstorage-proxy-as-loader](DEC-001-dstorage-proxy-as-loader.md), 2026-09-05, ship as a dstorage.dll proxy
