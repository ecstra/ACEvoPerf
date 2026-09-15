---
name: TODO-028-the-refresh-hold-test
kind: todo
description: two stints at 60 Hz in the same launch as TODO-025, windowed and then fullscreen, to show whether this laptop holds frames for a display refresh the frame rate is above, as the owner's desktop did, and whether fullscreen lets them out
updated: 2026-09-15
links: [BUG-009-one-percent-lows-far-below-average, TODO-025-the-ui-view-rotation-test, telemetry]
status: open
by: agent
area: render
born: 2026-09-15
done:
---

## What

Right after TODO-025's stint, in the same launch with the same car and track and the HUD schedule test still
on.

1. The game's monitor set to 60 Hz in Windows display settings, back in the game, 3 minutes of hot laps.
2. Fullscreen on in the game's video settings, still at 60 Hz, 3 minutes of hot laps.
3. Fullscreen off and the monitor back to 165 Hz afterwards.

Frames between `Entering occluded state` and `Exiting occluded state` in the game log, and the 20 seconds
after each return to the game, are dropped. Per stint, the frame time histogram, the share of frames within
0.4 ms of 16.67 ms and its multiples, the average and p99.

## Why

The owner's desktop read a 60 fps 1 percent low with the frame rate uncapped on a 60 Hz display, and 70 to
90 at 240 Hz (BUG-009). That is what frames held for the refresh look like. This laptop shows no hold at
165 Hz, where it runs below its refresh. At 60 Hz it runs above it, which is the desktop's condition, so
the laptop can show the hold and try fullscreen as the way out.

## Done when

BUG-009 records, for both 60 Hz stints against the 165 Hz stint of TODO-025, the average, p99 and the share
of frames at 16.7 ms steps. Frames bunched at 16.7 ms in the windowed stint, with p99 at or over 16.7 ms
while the average stays over 60 fps, show the hold on this machine, and the fullscreen stint says whether
fullscreen releases it. No bunching in the windowed stint means this laptop does not hold frames and the
desktop's reading belongs to its own presentation path.
