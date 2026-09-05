---
name: handover-2026-09-05-lap-analysed
kind: doc
description: state after the first telemetry lap was analysed, before lap two experiments
updated: 2026-09-05
links: [lap-2026-09-05-nordschleife, TODO-005-lap-two-experiments, TODO-003-capture-crash-evidence]
---

# Handover: lap analysed

## Where the work stands

- Mod 0.2.0 built and installed in the game folder. Ini in the game folder equals
  `dist/acevo_perf.ini` except `log_pso_on_creation=true`, which was turned on for the lap and
  should go back to commented out.
- Verified by measurement: staging cap frees about 1.5 GB of VRAM and grows both streaming pools
  from 400 to about 1115 MB, flag writes take effect, the swap chain hooks record frames, the
  timeline and frame CSVs and the GPU sampler cover a full lap.
- Written, not yet verified: every candidate change in TODO-005.
- Branch `feat/initial-mod` holds everything, main has no commits, no remote exists.

## Next step

Run the TODO-005 experiments one at a time, mod side ones first (tile queue priority, then
`texture_tier0`, then `ui_force_resource_preloading`), each as a short menu check
followed by a lap when it looks safe, and compare with `tools/telemetry_report.py`.

## Traps

- The game must be closed before `dist/install.ps1` can replace the DLL.
- PowerShell reserves `$pid`, and `python` resolves to a Store stub on the owner's machine, the
  session used `py` and `python3`.
- The report joins GPU samples on the clock second, so the sampler must run on the same machine
  clock as the game.
- Owner questions still open: does a crash show a dialog, freeze, or vanish, and which car and
  opponent count were used for the lap.
