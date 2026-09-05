---
title: Analyse the Nordschleife lap telemetry and turn it into concrete changes
status: in-progress
created: 2026-09-05
updated: 2026-09-05
source: user, "can you add some logging and open the game and tell me, i'll run a full lap around nordschliefe and tell you when done and you can read the logs and proceed from there? Ensure the logs work properly and log anything and everything you need (including fps if needed)"
---

## Done when

Each of the three complaints (low mip shown first, fps drop in new sections, general performance)
has either a measured cause with a change applied and re verified on a second lap, or a written
explanation of why it cannot be fixed from outside the game.

## Steps

1. Collect after the lap: `acevo_perf.log`, `acevo_perf_timeline.csv`, `acevo_perf_frames.csv`
   from the game folder, the newest `Saved Games\ACE\Logs\log-*.txt`, and `gpu.csv` from the
   session folder. Check: all five files present and covering the same clock range.
2. Frame time: percentiles, count of frames over 20 and 33 ms, and where in the lap they cluster.
   Check: hitches listed with clock times.
3. Correlate each drop with the same second in the timeline (tile and mesh volume, VRAM against
   budget, CPU) and the GPU sampler (clocks, power, throttle reasons) and the game log (PSO
   creation lines). Check: one named cause per drop cluster, or "unexplained".
4. Texture pop in: tile request batch sizes and per second volume during pop in. Check: whether the
   engine or the I/O path is the limit.
5. Apply the cheapest change per cause (flag, queue priority, setting), rebuild if needed, and ask
   for a second lap. Check: the same metric improves on lap two.

## Notes

- The lap was driven with `-log_debug=rendering` and the `log_pso_on_creation` flag on, so the game
  log is larger and noisier than usual. Logging itself can cost a little frame time.
