---
name: BUG-033-a-menu-view-remade-at-a-new-size-is-not-recognised
kind: bug
description: the menu and HUD view is identified by being first or by matching the first view's size, so one torn down and remade at a different size, which a fullscreen to windowed change does, is never recognised again and the page fixes stop reaching it for the rest of the session
updated: 2026-09-20
links: [responsive-ui, review-2026-09-fix-review-cohtml-build-guard, BUG-013-one-percent-lows-drop-after-window-or-input-switch, BUG-025-controls-page-scans-the-page-once-per-new-row, BUG-026-vehicle-setup-asks-for-the-setup-twice-per-open]
area: ui
status: open
severity: bug
reported: 2026-09-20
parent:
---

## Problem

`Hook_CreateView` in `src/ui/cohtml_hooks.cpp` tells the menu and HUD view from the car displays two ways.
The first view the game makes is it, and that view claims its own width and height so that any later view
of the same size is it again. The second rule exists so that a menu view torn down and remade is still
recognised, because the replacement arrives with a higher ordinal and the ordinal alone would never point
at it again.

Neither rule covers a remake at a different size. The claim holds the old size for the life of the
process, the remade view does not match it, and it is treated as a car display. It never receives the page
fixes script, so the controls page fix (BUG-025) and the vehicle setup fix (BUG-026) are gone for the rest
of the session, and the UI probe's `g_mainView` keeps pointing at the view that died.

This is the accepted residue of the fix for F-02 of
[the cohtml build guard review](../reviews/2026-09-fix-review-cohtml-build-guard/ledger.md), recorded here
rather than left in a ledger because it is a known limitation of shipped code.

## Evidence

Not reproduced. Twelve recorded sessions under `logs/`, including focus switches, pauses, a logged device
change and about fifty session loads, every one shows exactly one 1920x1080 view created and one script
install, so the underlying teardown has never been observed either.

What makes it worth filing rather than ignoring is the direction. The verifier pass of 2026-09-20 pointed
out that a fullscreen to windowed change, which is the device change BUG-013 already ties to a HUD reload,
remakes the view at the smaller client size. So the case most likely to trigger the teardown is also a
size change, which is exactly what is not covered.

## Reading

Two signals would close it, and both were declined for the branch that found it.

The swap chain size. `src/render/dxgi_hooks.cpp:19` sees `CreateSwapChainForHwnd` with the window size
about five seconds before the first view in every recorded log, and it sees a resize. Using it would make
the page fixes depend on `[dxgi] enabled`, and a setting silently disabling an unrelated fix is the defect
class that review exists to remove, so trading one for the other there was not worth it. If the render
branch ever separates the auto sizing from the measurement switch, this objection goes away.

The game window's client rectangle, found by walking the process's own top level windows. Self contained,
no dependency on another subsystem or on any setting, at the cost of about fifteen lines of window
enumeration and a question about what exists at the moment the first view is made.

## Fix

Absent.

## Verification

Absent.
