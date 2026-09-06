---
name: DEC-008-frame-latency-left-to-the-game
kind: decision
description: max_frame_latency ships at 0, the game's own swap chain latency stays, the proxy only logs the calls
updated: 2026-09-06
links: [DEC-006-frame-latency-cap-default, BUG-009-one-percent-lows-far-below-average, BUG-013-one-percent-lows-drop-after-window-or-input-switch]
date: 2026-09-05
area: render
status: standing
superseded-by:
---

## Decision

Default `max_frame_latency=0`. The proxy hooks `IDXGISwapChain2::SetMaximumFrameLatency` and
logs every call the game makes, but changes nothing unless the ini asks for a value.

## Why

The cap of DEC-006 never took effect. The game calls `SetMaximumFrameLatency(2)` on its waitable
swap chain right after creating it, a few microseconds after the proxy had set 1, and the proxy
never looked again. Every session up to 2026-09-05 21:19 ran at the game's own latency of 2, so
the pacing the owner liked on lap four was not the cap. The build of 21:19 forced the configured
value on every call, and the first session with 1 truly applied ran at 43 to 45 fps instead of
80 or more, with the process at 9 percent CPU: one frame of latency on a waitable swap chain
stops CPU and GPU work from overlapping on a GPU bound machine.

## Alternatives

- Keep forcing 1: rejected, halves the frame rate.
- Force 2 to pin the game's default against later changes: not yet, first the log has to show
  whether the game changes the value on a window switch or a session restart (BUG-013).

## Consequences

- The proxy leaves the swap chain's latency alone. The diagnostic knob that forced a value on
  every call was removed on 2026-09-06 with the other latency instruments: 3 measured 61 against
  57 fps on the 1 percent low in one lap and 59 in the next, inside run to run noise, at the
  cost of a frame of input lag.
- The pacing work in BUG-009 restarts from the game's real behaviour.
