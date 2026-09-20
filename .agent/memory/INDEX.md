---
name: memory-index
kind: doc
description: index of project memory, one line per fact
updated: 2026-09-20
links: [agent-index, spec-memory]
---

# Memory Index

project

- [reported-working-configurations](reported-working-configurations.md), the machines players report the mod working on and where each report came from, Nvidia and a Radeon RX 6600 and Linux through Proton, plus the open reports nobody has reproduced and why none of them points at a card
- [free-roam-is-a-password-away](free-roam-is-a-password-away.md), the Free Roam mode is gated by a plain text password and a hidden panel, its content stops at the Nürburgring complex, not shipped
- [ghost-car-records-but-never-shows](ghost-car-records-but-never-shows.md), the ghost car flag records, saves, loads and samples a lap and nothing in the exe draws it, not shipped
- [dstorage-dll-is-only-a-forwarder](dstorage-dll-is-only-a-forwarder.md), the runtime is dstoragecore.dll, the game claims that base name at start-up, nothing checks the two versions match, and the core's own entry points can be called directly
- [game-requests-1gb-staging-buffer](game-requests-1gb-staging-buffer.md), the one call that starves texture pools on small cards
- [release-build-ignores-gflags-cli](release-build-ignores-gflags-cli.md), only a whitelist of single dash switches is parsed, the per logger level switches among them
- [thermal-throttle-dominates-lap-fps](thermal-throttle-dominates-lap-fps.md), the reference GPU throttles at 87 °C two minutes into a lap, the undervolt that held it flat hung the GPU and came off again
- [reference-machine-has-no-direct-gpu-display](reference-machine-has-no-direct-gpu-display.md), no MUX and no output wired to the NVIDIA GPU, every frame crosses to the integrated GPU, never ask the owner for a way around it, and it is not what makes the 1 percent lows
- [handled-faults-stall-the-game](handled-faults-stall-the-game.md), the game's crash logger stalls a thread 120 to 210 ms on every access violation, even one the mod handles, so mod code never probes memory by faulting

reference

- [community-tools-assume-32mb-toc](community-tools-assume-32mb-toc.md), public kspkg tools read the wrong table size on 0.9.0

feedback

- [feedback-ship-as-installable-mod](feedback-ship-as-installable-mod.md), judge every change against a stranger installing from a zip
- [feedback-review-only-when-asked](feedback-review-only-when-asked.md), a code review runs only when the owner names one, and findings are fixed without the hunter and verifier
- [feedback-public-docs-are-for-players](feedback-public-docs-are-for-players.md), the owner called the readme and the changelog AI slop on 2026-09-15, the public files are written for players and checked against public-docs
