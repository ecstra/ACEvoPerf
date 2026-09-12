---
name: DEC-014-reflex-ships-on-boost-ships-off
kind: decision
description: Reflex ships enabled because the game has none and it costs nothing measurable, boost ships disabled because a thermally capped card has no clocks for it to hold up
updated: 2026-09-12
links: [reflex-2026-09-12, engine-flags]
date: 2026-09-12
area: render
status: standing
superseded-by:
---

## Decision

`reflex=1` and `reflex_boost=0` in the shipped ini.

Reflex is on because the game ships no Reflex at all, the driver confirms the mod's
integration works, and four two lap runs found no frame rate cost outside the run to run drift
of a thermally throttled card (`reflex-2026-09-12`). Its benefit is latency, which those runs
did not measure, so this is a decision made on "correct and free" rather than on a measured
win. One line in the ini turns it off.

Boost is off because it has nothing to do on the reference machine and cannot be shown to help
on any machine we have. It exists to stop a GPU dropping clocks while it waits, and the
reference card never waits: 97 percent utilised, pinned at 86 to 87 degrees, clocks limited by
heat rather than by a power saving state. Adding watts there buys nothing and costs thermal
headroom. It stays available for desktop users who have that headroom.

## Alternatives

- Ship Reflex off until a measured win exists: defensible, and rejected. The frame rate being
  unchanged is the expected result of a working Reflex integration, not evidence against one,
  and the reference machine is the wrong machine to prove latency on because it is GPU bound
  at its thermal limit. Shipping it off would mean nobody ever benefits from a capability the
  game does not otherwise have.
- Ship boost on, as the owner initially asked: rejected on the reasoning above and with the
  owner informed. The ini comment says plainly what it does and what it costs.
- Wire up `NvAPI_D3D_GetLatency` and decide on latency numbers: the right experiment, and not
  on this machine. Recorded as the restart check in the research doc.

## Consequences

- Reflex is NVIDIA only and silently idle elsewhere, which the log states once at start up.
  AMD and Intel users get nothing from this and no penalty either.
- Every frame carries one extra driver call. Measured across thousands of frames with zero
  refusals and no frame time cost that could be separated from clock drift.
- The reference machine cannot validate future latency work. Anything in this area needs a
  card with thermal headroom.
