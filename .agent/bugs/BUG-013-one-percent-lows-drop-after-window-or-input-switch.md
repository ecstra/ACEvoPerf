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

## Fix

Absent. Plan: the proxy hooks `XInputGetState`, `XInputGetCapabilities`, the DirectInput device
`Poll` and `GetDeviceState`, and `HidD_*` reads, counts calls and time per second into the
timeline CSV and logs any single call over 1 ms. One run with a window switch and one with a
controller to mouse change then shows whether the input path is the extra frame time. If it is,
the fix is a cache of the empty slots (skip re polling a slot that reported not connected for a
few seconds), which the proxy can do in the hook.

## Verification

Absent.
