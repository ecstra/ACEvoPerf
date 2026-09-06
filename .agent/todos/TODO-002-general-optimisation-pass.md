---
name: TODO-002-general-optimisation-pass
kind: todo
description: raise the lap frame rate on the 6 GB laptop GPU without visible quality loss
updated: 2026-09-06
links: [BUG-002-fps-drop-entering-new-track-sections, BUG-009-one-percent-lows-far-below-average, lap-2026-09-05-nordschleife, one-percent-low-hunt-2026-09-05, engine-flags, telemetry]
status: done
by: owner
area: render
born: 2026-09-05
done: 2026-09-06
---

## What

"And ofc, general optimization needed as well."

## Why

The lap of 2026-09-05 shows the GPU pinned at 100 percent and thermally throttled to about
1500 MHz. Frame rate follows GPU work per frame and cooling, nothing else was near a limit.

## Done when

Every engine inefficiency the mod can correct on this machine is either fixed in the default
ini or written off with a measurement. Closed 2026-09-06: the candidates below are all
written off, the fixes the pass found earlier (the staging buffer cap, the fixed pools, the
PSO cache flag, the process tweaks) are the default ini, and what remains on this machine is
the GPU bound frame rate (BUG-002), the present path through the integrated GPU and the
render thread's late frames in heavy views (BUG-009, parked with TODO-010). Settings profiles are out (owner, 2026-09-06: the mod
fixes what the engine gets wrong, it does not ship knobs), so the earlier candidates (DLSS
Quality, clouds, volumetrics, motion blur and grass one step down, fewer GI probes) are the
owner's own menu choices and not this item.

## Candidates, 2026-09-06

- C++ exceptions on the render thread: cleared on 2026-09-06, lap 21. The throw log hooked
  the exe's `_CxxThrowException` import for a whole session, menu and lap, and counted zero
  throws. The unwinding samples of the sampler came from loading minutes (54 against 3 in a
  loading minute, 8 against 1 in a driving minute) and the vcruntime range under
  `__NLG_Return2` also covers the funclet caller the unwinder uses for exceptions thrown by
  the UI DLLs, so nothing the game's own code does per frame.
- The tiled instances buffer: cleared on 2026-09-06, lap 22. With `dx12_instances_tiled=false`
  the game commits the whole 384 MB up front ("monolithic path" in its log) and the first
  minute from the pit exit ran 86.0 fps with a 61.9 fps 1 percent low against 87.8 and 63.8
  on lap 21 with the tiled buffer, inside run to run noise, with 160 MB more VRAM in use.
  The tiled path stays.
- DirectStorage BypassIO: queried through `FSCTL_MANAGE_BYPASS_IO` on `content.kspkg`,
  status success, no failing driver, the file uncompressed, NTFS on the Samsung NVMe. The
  runtime's fast path is available, nothing to enable.
- Machine state read the same day: High performance power plan, Game Mode off, Ryzen 9
  5900HX with 8 cores and 16 threads. Nothing there for the mod.
- Thread pools are a rule, not a fault: two render, two physics and one loading worker, then
  every logical core beyond eight adds one in the order physics, render, physics, render,
  loading, so 16 logical cores give 6, 5 and 2 (`minimumcores=true` stops at the base).
  Nothing to fix there without a code patch, parked.
- The present path through the integrated GPU costs about a millisecond a frame and more in
  slow frames (BUG-009). Hardware wiring, not the mod's.
