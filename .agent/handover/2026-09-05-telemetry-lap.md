---
title: Telemetry lap in progress
date: 2026-09-05
branch: feat/initial-mod
---

## State

- Mod 0.2.0 built and installed in the game folder with `staging_buffer_mb=128`,
  `enable_pso_cache=true`, `no_intro=true`, `log_pso_on_creation=true` (diagnostic, remove after),
  frame stats and timeline on.
- Verified this session: staging cap frees about 1.5 GB VRAM and grows both streaming pools from
  400 to about 1115 MB, flag writes take effect (`minimumcores` changed the thread pools,
  `force_canonical_pool_sizes` printed the canonical lines), the swap chain hooks record frames.
- Repo initialised on `feat/initial-mod`, nothing merged to main yet.

## In flight

- The user is driving one full Nordschleife lap with the game launched as
  `AssettoCorsaEVO.exe -log_debug=rendering`. A `nvidia-smi` sampler writes `gpu.csv` into a
  session folder outside the repo (`acevo-logs/lap-nordschleife-<stamp>` next to the repo, the
  path is in `acevo-logs/current_session.txt`). When the user says done: stop the sampler (pid in
  `gpu_sampler.pid`), copy the three mod files and the newest game log into the session folder,
  then follow `.agent/todos/analyse-nordschleife-lap-telemetry.md`.

## Open questions

- Did the menu icon problem and the crashes exist before the mod was installed?
- Which car and how many opponents were used for the lap?

## Pointers

- Bugs: all seven files under `.agent/bugs/` are open, two are being investigated with the lap.
- Todos: analysis, general optimisation, crash evidence, release packaging.
- Decisions: the four records dated 2026-09-05 explain the current defaults.
