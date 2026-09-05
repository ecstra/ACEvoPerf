---
name: DEC-006-frame-latency-cap-default
kind: decision
description: max_frame_latency=1 shipped on by default, superseded the same day, the cap never took effect
updated: 2026-09-05
links: [BUG-009-one-percent-lows-far-below-average, proxy-architecture, DEC-008-frame-latency-left-to-the-game]
date: 2026-09-05
area: render
status: superseded
superseded-by: DEC-008-frame-latency-left-to-the-game
---

Superseded by DEC-008 on 2026-09-05: the game set its own latency of 2 right after the proxy's
call, so this cap was never in effect, and once the proxy enforced it the frame rate halved.

## Decision

Default `max_frame_latency=1` in `dist/acevo_perf.ini`. The game already creates its swap chain
with the waitable object and tearing flags (`flags=0x840` in the proxy log), so the proxy only
calls `SetMaximumFrameLatency(1)` on it. Verified on lap four of 2026-09-05: the owner reported
steadier pacing at the same average frame rate as lap three.

## Alternatives

- Leave the game's queue depth: three frames of CPU run ahead, then a stall when the GPU falls
  behind, which is part of the every other frame alternation in BUG-009.
- A frame rate cap: also flattens the curve but costs the peaks, kept as an option for the owner.

## Consequences

- Lower input latency and less run ahead. On a CPU bound machine this could cost average frame
  rate, the reference machine is GPU bound and showed none.
