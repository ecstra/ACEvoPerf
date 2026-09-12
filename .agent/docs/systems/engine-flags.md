---
name: engine-flags
kind: doc
description: the engine's gflags, which ones matter, and how the mod sets them
updated: 2026-09-12
links: [DEC-002-flags-by-memory-write, DEC-009-pool-and-staging-sizes-by-card, release-build-ignores-gflags-cli, proxy-architecture]
---

# Engine flags

The engine declares 216 gflags (126 bool, 32 double, 31 string, 27 int32). Names, files,
defaults and help text are in `tools/data/gflags_full.tsv`, recovered from the exe. The release build
does not parse them from the command line (see the memory `release-build-ignores-gflags-cli`), so
the mod writes their storage directly (DEC-002, `ScanFlags` in `src/engine/flags.cpp`). The runtime
scan finds 203 of them on 0.9.0 in about 70 ms.

Any bool, int32 or double name from the table works in the `[flags]` section of the ini as
`name=value`. Unknown names and string flags are reported in `acevo_perf.log` and skipped.

## Flags that matter for performance

| flag | default | effect | status |
|---|---|---|---|
| `enable_pso_cache` | false | pipeline state cache on disk, fewer shader stalls | shipped on until 0.3.1, off since, BUG-015 |
| `no_intro` | false | skip intro scenes | on by default in the mod |
| `force_canonical_pool_sizes` | false | fixed pools instead of the dynamic budget (1433 MB each on its own) | on by default, DEC-005 |
| `tile_pool_mb` | 0 | tile pool size in MB, honoured only with the canonical path, created once at start | `auto` by default, 1024, 1536, 2048 or 3072 by the card's memory, DEC-005 and DEC-009 |
| `texture_tier0` | false | "Force Texture Tier 0" pins every texture to its lowest tier, tile streaming stops | never enable |
| `ui_force_resource_preloading` | false | preloads 1037 interface files (181 MB) at start, no measurable effect | measured, off |
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
with 1433 MB pools, both on 2026-09-05. The pool sizes follow a rule read from the exe on
2026-09-06: two render, two physics and one loading worker always, then one more worker per
logical processor beyond eight in the order physics, render, physics, render, loading, so 16
logical processors give 6, 5 and 2. `minimumcores` skips the extra workers. `no_intro` has no visible trace in the log yet.
`texture_tier0=true` on a four minute drive of 2026-09-05 cut tile traffic to 1.3 GB with whole
minutes at 1 MB (lap one moved 8.3 GB in fourteen minutes) and the owner saw the car stuck at
low detail. `ui_force_resource_preloading=true` logged the preload but the owner felt no change.

## The scan survives a game update

The game went to 0.9.1+release.6 (exe of 2026-09-07, installed by Steam on 2026-09-11) and the
mod's scan needed no change: `scanned exe in 78 ms: 4 ctor candidates, 204 flags (204 typed)`,
one flag more than 0.9.0's 203, the same four constructor candidates, and all four shipped
flags written at their new addresses with the staging cap and the 1024 MB pool applied. The
addresses moved, the names did not, which is what the scan is for. The one flag 0.9.1 adds has
not been identified, `tools/data/gflags_full.tsv` is still the 0.9.0 dump.

That update also moved the owner's install from a folder outside Steam to
`steamapps\common\Assetto Corsa EVO`, and it did not replace `dstorage.dll`, so the mod kept
running across it.
