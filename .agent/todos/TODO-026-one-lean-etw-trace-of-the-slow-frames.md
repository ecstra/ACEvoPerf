---
name: TODO-026-one-lean-etw-trace-of-the-slow-frames
kind: todo
description: one Windows performance trace of a parked minute and a lap, started by the owner from an elevated prompt, to name what the present thread waits on and who wakes it in BUG-009's slowest frames, its game code by RVA and the 60 Hz beat, replacing every in process instrument the hunt used to bring back
updated: 2026-09-14
links: [BUG-009-one-percent-lows-far-below-average, one-percent-lows-2026-09-14, TODO-010-resume-the-one-percent-low-hunt, telemetry, tools]
status: open
by: agent
area: tooling
born: 2026-09-14
done:
---

## What

The second run from BUG-009's deep dive
([one-percent-lows-2026-09-14](../docs/research/one-percent-lows-2026-09-14.md)). No build.

- **The profile.** A lean Windows Performance Recorder profile: context switches and ready thread rows
  with stacks, 1 kHz CPU samples, DPC and ISR rows, thread names, focus changes, and the DXGI and DxgKrnl
  present events without stacks. The heavy stock `CPU` plus `GPU` profiles add system calls, faults, disk
  IO and stacks on every graphics event, an estimated 0.3 to 0.35 ms a frame and 4 to 10 GB. The lean one
  is estimated at 0.13 ms a frame and 1.5 to 3 GB for about 190 s. The profile and its parser were drafted
  in the deep dive and move into `tools/` and `tools/data/` under their own gates first.
- **A pre check with no game.** One minute from an elevated PowerShell, start the profile, wait 10 s,
  stop it, and decode it, which proves kernel rows, stack association, thread names, lost events and the
  data rate.
- **The run.** The run M protocol of 2026-09-13, Nürburgring GP pit box, shipped ini with `frames=1`.
  Parked 20 s after the HUD, 60 s untraced, then the trace starts (drop its first 5 s), 60 s traced parked,
  one traced lap, the trace stops at the line. The trace is accepted if the median moves under 0.2 ms and
  p99 over median under 0.02 across the untraced and traced halves, and void if a focus change falls in a
  traced window.
- **It needs the owner.** Kernel recording needs elevation, and agents on this machine get access denied.
  About 3 to 6 GB free for the trace and its merge copy.

## Why

The frames on disk say the slowest frames are the present path, spread out game code and a UI ripple,
but no in process instrument can see what the present thread waits on, who readies it, whether its SMT
sibling was busy, or what the game code excess is, and the old sampler moved the very path it measured.
One trace covers the fence hooks, the census, the sampler and the affinity script the hunt would have
brought back.

## Done when

The pre check decodes kernel rows with stacks and no lost events, the game trace passes its own cost check,
and BUG-009 records, for the slowest 1 percent against the median half of the present thread's frames,
what it waits on with the wait site and the readying thread as RVAs, its ready time and sibling use, the
game code excess by RVA, and the 60 Hz source if that launch shows the beat.
