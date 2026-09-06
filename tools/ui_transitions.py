"""Cost of every UI transition, from the mod's frames CSV lined up with the game log.
Usage: ui_transitions.py FRAMES_CSV MOD_LOG GAME_LOG [--window 2.0]
For each page marker in the game log (page loads, pause menu, settings pages, bindings groups):
the frames in the window that follows, the worst frame, the frames over 33 ms and the total time
above 16.7 ms. Then the median and p99 frame time of every stretch between markers."""
import re
import sys
from datetime import datetime

frames_csv, mod_log, game_log = sys.argv[1:4]
window = float(sys.argv[sys.argv.index("--window") + 1]) if "--window" in sys.argv else 2.0

first = open(mod_log, encoding="utf-8", errors="replace").readline()
m = re.match(r"\[(\d\d):(\d\d):(\d\d)\.(\d\d\d)\]", first)
attach = int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]) + int(m[4]) / 1000.0

frames: list[tuple[float, float]] = []
with open(frames_csv, encoding="utf-8") as f:
    next(f)
    for line in f:
        parts = line.split(",")
        frames.append((attach + float(parts[0]), float(parts[1])))

events: list[tuple[float, str]] = []
pat = re.compile(r"\[\d{4}-\d\d-\d\d (\d\d):(\d\d):(\d\d)\.(\d\d\d)\] \[gameface\] \[info\] (Loading page [^\n]*?|PauseMenu Show|KS-PAGE-SETTINGS-CONTROLS page \w+|Showing bindings for \w+|remove opaque [\w-]+)\s*$")
with open(game_log, encoding="utf-8", errors="replace") as f:
    for line in f:
        mm = pat.match(line)
        if mm:
            t = int(mm[1]) * 3600 + int(mm[2]) * 60 + int(mm[3]) + int(mm[4]) / 1000.0
            events.append((t, mm[5].strip()))


def clock(t: float) -> str:
    return f"{int(t // 3600):02d}:{int(t % 3600 // 60):02d}:{t % 60:06.3f}"


print(f"{'event':60s} {'frames':>6s} {'worst':>7s} {'>33ms':>5s} {'stall':>7s}")
for t, name in events:
    win = [ms for (ft, ms) in frames if t <= ft < t + window]
    if not win:
        continue
    worst = max(win)
    over = sum(1 for ms in win if ms > 33)
    stall = sum(max(0.0, ms - 16.7) for ms in win)
    print(f"{clock(t)} {name:47.47s} {len(win):6d} {worst:6.0f}ms {over:5d} {stall:6.0f}ms")

print()
print("steady stretches (from 1 s after each event to the next event):")
for i, (t, name) in enumerate(events):
    end = events[i + 1][0] if i + 1 < len(events) else t + 30
    win = sorted(ms for (ft, ms) in frames if t + 1.0 <= ft < end)
    if len(win) < 20:
        continue
    med = win[len(win) // 2]
    p99 = win[int(len(win) * 0.99)]
    print(f"{clock(t)} {name:47.47s} {len(win):5d} frames  median {med:5.1f} ms  p99 {p99:6.1f} ms  ({1000 / med:4.0f} fps)")
