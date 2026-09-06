---
name: BUG-013-one-percent-lows-drop-after-window-or-input-switch
kind: bug
description: the 1 percent low frame rate drops for the rest of the stint after switching window or after changing between controller and mouse
updated: 2026-09-06
links: [BUG-009-one-percent-lows-far-below-average, TODO-010-resume-the-one-percent-low-hunt, telemetry, proxy-architecture]
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

- 21:26 session, twenty minutes with the swap chain hooks logging and the game's own latency:
  the game never touched `SetMaximumFrameLatency`, `ResizeBuffers` or `SetFullscreenState` after
  startup, and the GPU clock stayed at about 1560 MHz through a switch. Every frame over 80 ms
  lines up with a discrete event in the game log:

  | event | stall |
  | --- | --- |
  | session load | up to 1.2 s |
  | window loses focus: the game pauses itself and loads the pause page | 130 to 170 ms |
  | focus returns: resume, the HUD page reloads | 90 to 150 ms, two or three in a row |
  | back to pits: dynamic track preset load (BUG-012) | 1.5 s |
  | a device change at 21:37:52: all 21 DirectInput devices recreated, audio system restarted | 660 ms |

  The owner's overlay computes its 1 percent low over a rolling window, so each of these pulls
  the reading down and it recovers when the stall leaves the window, which is the "drops and
  comes back" pattern. The steady state after a switch is unchanged (BUG-009 holds the clean
  driving numbers).
- What triggered the 21:37:52 device change is not known yet. The exe does not call
  `RegisterDeviceNotificationW`, so the DirectInput rebuild answers the plain `WM_DEVICECHANGE`
  broadcast (or FMOD's own device list callback for the audio side). The proxy now logs every
  device interface arrival and removal and every audio endpoint change with its time.

- Sessions 22:00 to 08:52 (laps 9 to 19): the device watch logged no device change during a
  lap, and no lap reproduced the drop without a pause or a session restart in the window. The
  input probe and the device watch were removed on 2026-09-06 with the rest of the diagnostics
  (BUG-009), the device events are still in the logs of those sessions.

## Fix

Absent. Each stall has its own owner: the pause page and HUD reloads belong to the UI (out of
scope for now), the pit lane return is BUG-012, the device rebuild needs the device named on the
day it happens again. If an audio endpoint flaps (a virtual device of a sound utility), the fix
is on the machine, or a window procedure filter that swallows device change broadcasts that
carry no game controller. The device watch code is in the history before the removal commit,
TODO-010 lists it.

## Verification

Absent.
