---
name: handover-2026-09-05-four-laps-done
kind: doc
description: state after four telemetry laps, fixed pools and the latency cap are the defaults
updated: 2026-09-05
links: [handover-2026-09-05-lap-analysed, lap-2026-09-05-nordschleife, TODO-002-general-optimisation-pass, TODO-004-release-packaging]
---

# Handover: four laps done

Continues [2026-09-05-lap-analysed](2026-09-05-lap-analysed.md).

## Where the work stands

- Installed in the game folder: mod 0.2.0 with the defaults of `dist/acevo_perf.ini` (staging
  128 MB, fixed 1024 MB tile pool with the canonical mesh cap, PSO cache, intro skip, one frame
  of swap chain latency, telemetry on). The game folder ini additionally has `veh_crashdumps`
  on, harmless.
- Verified by the owner and the logs: crashes gone (BUG-004, BUG-005), icons fine (BUG-003),
  road sharp and stable across restarts with texture quality Ultra (BUG-007, BUG-010), pacing
  steadier (BUG-009 partial).
- Written, not verified: the `gpu-relief` settings profile, the `pacing` profile's fullscreen
  half (the owner saw no difference), `tile_pool_mb` guidance for 8 and 12 GB cards.
- Branch `feat/initial-mod`, all commits in house style from the fourth one on, main empty, no
  remote.

## Next step

TODO-002: measure the `gpu-relief` profile on one lap for the average and the section spread,
then decide the recommended profile. TODO-004: release zip. BUG-001 (low mips on first sight) has
no engine knob found yet, BUG-006 is the grass distance.

## Traps

- `texture_tier0` and `minimumcores` must never be enabled, both are documented in engine-flags.
- The engine's own pool sizing runs during transitions, so any pool measurement must be read
  from the race, not the menu.
- The GPU sampler must be stopped by pid after each session, see the session folders under
  `logs/`.
