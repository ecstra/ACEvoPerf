---
name: TODO-001-analyse-nordschleife-lap-telemetry
kind: todo
description: read the lap telemetry and turn each complaint into a measured cause and a change
updated: 2026-09-05
links: [lap-2026-09-05-nordschleife, BUG-001-texture-low-mip-shown-before-streaming, BUG-002-fps-drop-entering-new-track-sections, TODO-005-lap-two-experiments]
status: open
by: owner
area: streaming
born: 2026-09-05
done:
---

## What

"can you add some logging and open the game and tell me, i'll run a full lap around nordschliefe
and tell you when done and you can read the logs and proceed from there? Ensure the logs work
properly and log anything and everything you need (including fps if needed)."

## Why

The three complaints (low mip shown first, fps drop in new sections, general performance) need
data before any change.

## Done when

Each complaint has a measured cause written into its bug file and a change queued in
TODO-005. The lap analysis lives in the research doc `lap-2026-09-05-nordschleife`. Logging and
the lap are done, the analysis doc and the changes close this item.
