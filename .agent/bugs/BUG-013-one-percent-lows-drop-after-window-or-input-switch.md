---
name: BUG-013-one-percent-lows-drop-after-window-or-input-switch
kind: bug
description: the 1 percent low frame rate drops for the rest of the stint after switching window or after changing between controller and mouse
updated: 2026-09-05
links: [BUG-009-one-percent-lows-far-below-average, telemetry, proxy-architecture]
status: open
severity: bug
area: render
reported: 2026-09-05
parent:
---

## Problem

Owner wording: "The 1% genuinely drops when I switch either the window or the control
(controller -> mouse and mouse -> controller)." And after the 2026-09-05 20:55 session: "It was
~70fps 1% and dropped to 55ms after a switch (going out and coming back). Cant say it was gpu this
time (cuz its not)."

## Evidence

- Lap six (19:55): before the switch 98 fps with a 77 fps 1 percent low, after it 81 with 53, but
  the section after the return streamed 800 MB and the GPU sat at 99 percent, so the section
  explained it (BUG-009). The owner now reports the same drop with no GPU reason and also on an
  input device change with no window switch, which the section story cannot cover.
- 20:55 session: GPU clock pinned at 1822 MHz, 65 to 69 C, no throttling, so heat is out. The
  stint was too short and cut by pauses for a clean before and after number.
- The exe imports `XINPUT1_4.dll`, `DINPUT8.dll` and `HID.DLL`. Both triggers (focus change,
  device change) are events where an input system re enumerates devices and can start polling
  every slot every frame. `XInputGetState` on an empty controller slot is known to cost
  milliseconds per call, and DirectInput device polls stall on some drivers. That is the first
  thing to measure, not a conclusion.

- Owner, later the same evening: the drop also comes after restarting a session several times in
  a row, and in both cases the 1 percent low "goes back to pre-fix era", the alternating pacing
  that the latency cap (DEC-006) removed. That points at the cap itself being undone: the game
  runs its own waitable swap chain and may re apply its own frame latency, or recreate or resize
  the swap chain, on a focus change or a session restart. The mod set the latency once at swap
  chain creation and never looked again.

- 21:22 session with the hooks in place: the game calls `SetMaximumFrameLatency(2)` once, right
  after the swap chain exists. Forcing 1 there ran the game at 43 to 45 fps (DEC-008), so the
  "pre fix era" the owner sees is not a lower latency. The input probe counted 86 to 90 polls per
  second costing about 1 ms per second, slowest poll 0.09 ms, in the menu and the race, so
  controller polling is not the cost either, at least before a device change.

## Fix

Absent. The proxy now logs every `SetMaximumFrameLatency`, `ResizeBuffers` and
`SetFullscreenState` call with a timestamp and leaves the values alone (DEC-008). The next run
does the window switch and the repeated session restarts and reads the log against the frames
CSV: a latency or swap chain change at those moments is the root cause, no change means the
present path itself (composed against flip presentation after focus loss) is next, readable
through `GetFrameStatistics` or PresentMon.

## Verification

Absent.
