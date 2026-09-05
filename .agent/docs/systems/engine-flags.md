---
name: engine-flags
kind: doc
description: the engine's gflags, which ones matter, and how the mod sets them
updated: 2026-09-05
links: [DEC-002-flags-by-memory-write, release-build-ignores-gflags-cli, proxy-architecture]
---

# Engine flags

The engine declares 216 gflags (126 bool, 32 double, 31 string, 27 int32). Names, files,
defaults and help text are in `tools/gflags_full.tsv`, recovered from the exe. The release build
does not parse them from the command line (see the memory `release-build-ignores-gflags-cli`), so
the mod writes their storage directly (DEC-002, `ScanFlags` in `src/dllmain.cpp`). The runtime
scan finds 203 of them on 0.9.0 in about 70 ms.

Any bool, int32 or double name from the table works in the `[flags]` section of the ini as
`name=value`. Unknown names and string flags are reported in `acevo_perf.log` and skipped.

## Flags that matter for performance

| flag | default | effect | status |
|---|---|---|---|
| `enable_pso_cache` | false | pipeline state cache on disk, fewer shader stalls | on by default in the mod |
| `no_intro` | false | skip intro scenes | on by default in the mod |
| `force_canonical_pool_sizes` | false | fixed 1433 MB texture and mesh pools instead of the dynamic budget | documented option, DEC-004 |
| `tile_pool_mb` | 0 | tile pool size, only consulted with the canonical path | documented |
| `texture_tier0` | false | "Force Texture Tier 0", semantics untested | TODO-005 |
| `ui_force_resource_preloading` | false | preload UI resources as dev builds do | TODO-005, BUG-008 |
| `gibake_probes_per_frame` | 16 | GI probes rendered per frame | TODO-002 |
| `car_update_animations_budget`, `car_update_complete_budget` | 3, 2 | non focused cars updated per frame | documented |
| `disable_dynamic_track` | false | skip rubber and marbles simulation | documented |
| `disable_vrs` | false | variable rate shading off | documented |
| `minimumcores` | false | shrinks thread pools 5/6/2 to 2/2/1, measured | never enable |
| `log_pso_on_creation` | false | one debug line per pipeline state object | diagnostics |
| `veh_crashdumps`, `dumplevel` | false, 2 | the game's own crash dumps | TODO-003 |

## Verified effects

`minimumcores=true` changed the game log's thread line from `Render: 5 - Physics: 6 - Loading: 2`
to `2 - 2 - 1`, and `force_canonical_pool_sizes=true` produced the `canonical sizes forced` lines
with 1433 MB pools, both on 2026-09-05. `no_intro` has no visible trace in the log yet.
