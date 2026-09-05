#!/usr/bin/env python3
"""telemetry_report.py - summarise one game session recorded by ACEvoPerf.

Reads from a session folder (see .agent/docs/telemetry.md):
  acevo_perf_timeline.csv, acevo_perf_frames.csv, acevo_perf.log, gpu.csv (nvidia-smi sampler)
  and the game log (log-*.txt). All files are optional except the timeline.

Usage: telemetry_report.py SESSION_DIR [--slow-fps N] [--from HH:MM:SS] [--to HH:MM:SS]
"""
import argparse
import csv
import glob
import os
import re
import statistics
from collections import Counter, defaultdict
from dataclasses import dataclass
from typing import Optional


@dataclass
class GpuSample:
    util: int
    clock: int
    power: float
    temp: int
    reasons: int
    sw_thermal: bool
    sw_power: bool
    hw_thermal: bool


def read_timeline(path: str) -> list[dict[str, str]]:
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def read_frames(path: str) -> list[tuple[float, float, int, int, float, float, float, float, float, int]]:
    """(seconds since attach, frame ms, tile requests, GPU uploads, previous Present call ms,
    wait ms, fence wait ms, tile mapping ms, ExecuteCommandLists ms, tiles mapped) per presented
    frame. The later columns read as zero in older files."""
    if not os.path.exists(path):
        return []
    with open(path, newline="") as f:
        return [(float(row["t_s"]), float(row["frame_ms"]), int(row.get("tile_req") or 0), int(row.get("gpumem_req") or 0),
                 float(row.get("present_ms") or 0.0), float(row.get("wait_ms") or 0.0), float(row.get("fence_ms") or 0.0),
                 float(row.get("tilemap_ms") or 0.0), float(row.get("execute_ms") or 0.0), int(row.get("mapped_tiles") or 0))
                for row in csv.DictReader(f)]


def print_spread(stamped: list[tuple[float, float, int, int]]) -> None:
    """How wide the frame time distribution is and whether the slow frames carry streaming."""
    ms = [fr[1] for fr in stamped]
    median = statistics.median(ms)
    total = sum(ms)
    print(f"median {median:.2f} ms, p99/median {percentile(ms, 99) / median:.2f}")
    for factor in (1.3, 1.5, 2.0):
        slow = [fr for fr in stamped if fr[1] > factor * median]
        above = sum(fr[1] - median for fr in slow)
        print(f"frames over {factor}x median ({factor * median:.1f} ms): {len(slow):5d} ({100.0 * len(slow) / len(ms):.2f} %), "
              f"time above the median {above:.0f} ms ({100.0 * above / total:.1f} % of the window)")
    has_requests = any(fr[2] or fr[3] for fr in stamped)
    if has_requests:
        n1 = max(1, len(stamped) // 100)
        slowest = sorted(stamped, key=lambda fr: -fr[1])[:n1]
        all_tiles = sum(1 for fr in stamped if fr[2] > 0)
        slow_tiles = sum(1 for fr in slowest if fr[2] > 0)
        slow_uploads = sum(1 for fr in slowest if fr[3] > 0)
        print(f"slowest 1% ({n1} frames): {slow_tiles} with tile requests, {slow_uploads} with GPU uploads, "
              f"against {100.0 * all_tiles / len(stamped):.1f} % of all frames with tile requests")
    if any(fr[4] > 0 for fr in stamped):
        n1 = max(1, len(stamped) // 100)
        slowest = sorted(stamped, key=lambda fr: -fr[1])[:n1]
        blocked = sum(1 for fr in slowest if fr[4] > 0.5 * fr[1])
        print(f"Present call: median {statistics.median(fr[4] for fr in stamped):.2f} ms over all frames, "
              f"{blocked} of the slowest {n1} frames spent over half their time inside Present")
        if any(fr[5] > 0 for fr in stamped):
            faster = sorted(stamped, key=lambda fr: fr[1])[:len(stamped) // 2]
            def split(group):
                return (statistics.fmean(fr[1] for fr in group), statistics.fmean(fr[6] for fr in group),
                        statistics.fmean(fr[5] - fr[6] for fr in group), statistics.fmean(fr[4] for fr in group),
                        statistics.fmean(fr[1] - fr[5] - fr[4] for fr in group))
            for label, group in (("slowest 1%", slowest), ("faster half", faster)):
                total, fence, other, present, rest = split(group)
                print(f"{label:12} frame {total:5.1f} ms = GPU fence wait {fence:4.1f} + other waits {other:4.1f} + present {present:4.1f} + render thread work {rest:5.1f}")
            if any(fr[9] > 0 or fr[8] > 0 for fr in stamped):
                for label, group in (("slowest 1%", slowest), ("faster half", faster)):
                    with_maps = sum(1 for fr in group if fr[9] > 0)
                    print(f"{label:12} queue: tile mappings in {100.0 * with_maps / len(group):4.1f} % of frames, "
                          f"{statistics.fmean(fr[9] for fr in group):6.1f} tiles and {statistics.fmean(fr[7] for fr in group):4.2f} ms per frame, "
                          f"ExecuteCommandLists {statistics.fmean(fr[8] for fr in group):4.2f} ms per frame")


def print_gpu_split(path: str, stamped: list[tuple]) -> None:
    """GPU time per frame from the timestamp marks, the slowest 1% of frames against the faster half."""
    if not os.path.exists(path) or not stamped:
        return
    with open(path, newline="") as f:
        rows = {float(r["t_s"]): (int(r["submits"]), float(r["gpu_busy_ms"]), float(r["gpu_span_ms"]), float(r["gpu_lag_ms"]))
                for r in csv.DictReader(f)}
    if not rows:
        return
    n1 = max(1, len(stamped) // 100)
    slowest = sorted(stamped, key=lambda fr: -fr[1])[:n1]
    faster = sorted(stamped, key=lambda fr: fr[1])[:len(stamped) // 2]
    print("\n== GPU time per frame (timestamp marks on the present queue) ==")
    for label, group in (("slowest 1%", slowest), ("faster half", faster), ("all frames", stamped)):
        hits = [rows[fr[0]] for fr in group if fr[0] in rows]
        if not hits:
            print(f"{label:12} no GPU rows")
            continue
        print(f"{label:12} {len(hits):5d} of {len(group)} frames with GPU rows: {statistics.fmean(h[0] for h in hits):4.1f} batches, "
              f"GPU busy {statistics.fmean(h[1] for h in hits):5.2f} ms, span {statistics.fmean(h[2] for h in hits):5.2f} ms, "
              f"lag behind the CPU {statistics.fmean(h[3] for h in hits):5.2f} ms (max {max(h[3] for h in hits):5.2f})")
    matched = [(fr[1], rows[fr[0]][1]) for fr in stamped if fr[0] in rows]
    if len(matched) > 100:
        frame_ms = [m[0] for m in matched]
        busy = [m[1] for m in matched]
        print(f"correlation frame time vs GPU busy time: {statistics.correlation(frame_ms, busy):+.2f} over {len(matched)} frames")


def print_sample_mix(path: str, t_lo: float, t_hi: float) -> None:
    """Where the render thread was, by module, in the slowest 1% of frames against the rest."""
    if not os.path.exists(path):
        return
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        buckets = [name for name in reader.fieldnames if name not in ("t_s", "frame_ms")]
        rows = [r for r in reader if t_lo <= float(r["t_s"]) <= t_hi]
    if len(rows) < 100:
        return
    rows.sort(key=lambda r: -float(r["frame_ms"]))
    n1 = max(1, len(rows) // 100)
    groups = [("slowest 1%", rows[:n1]), ("median half", rows[len(rows) // 2:])]
    print("\n== render thread samples by module (share of samples, mean samples per frame) ==")
    print(f"{'':12}" + "".join(f"{b:>10}" for b in buckets) + f"{'per frame':>11}")
    for label, group in groups:
        totals = {b: sum(int(r[b]) for r in group) for b in buckets}
        all_samples = sum(totals.values())
        if all_samples == 0:
            continue
        print(f"{label:12}" + "".join(f"{100.0 * totals[b] / all_samples:9.1f}%" for b in buckets) + f"{all_samples / len(group):11.1f}")
    slow_totals = {b: sum(int(r[b]) for r in rows[:n1]) for b in buckets}
    fast_totals = {b: sum(int(r[b]) for r in rows[len(rows) // 2:]) for b in buckets}
    per_slow = {b: slow_totals[b] / n1 for b in buckets}
    per_fast = {b: fast_totals[b] / len(rows[len(rows) // 2:]) for b in buckets}
    extra = sorted(((per_slow[b] - per_fast[b], b) for b in buckets), reverse=True)
    print("extra samples per slow frame against a median frame: " + ", ".join(f"{b} +{d:.1f}" for d, b in extra[:4] if d > 0))


def low_fps(values: list[float], fraction: float) -> float:
    """The '1% low' style number: mean of the slowest fraction of frames, as fps."""
    if not values:
        return 0.0
    s = sorted(values, reverse=True)
    n = max(1, int(len(s) * fraction))
    return 1000.0 / mean(s[:n])


def read_gpu(path: str) -> dict[str, GpuSample]:
    """nvidia-smi csv keyed by HH:MM:SS (last sample of each second wins)."""
    out: dict[str, GpuSample] = {}
    if not os.path.exists(path):
        return out
    with open(path, newline="") as f:
        for row in csv.reader(f):
            if len(row) < 14 or not row[0].strip()[0:4].isdigit():
                continue
            cells = [c.strip() for c in row]
            clock_key = cells[0].split(" ")[1][:8]
            num = lambda s: int(re.sub(r"[^0-9]", "", s) or 0)
            out[clock_key] = GpuSample(
                util=num(cells[1]),
                clock=num(cells[4]),
                power=float(re.sub(r"[^0-9.]", "", cells[6]) or 0),
                temp=num(cells[7]),
                reasons=int(cells[9], 16) if cells[9].startswith("0x") else 0,
                sw_thermal=cells[12] == "Active",
                sw_power=cells[10] == "Active",
                hw_thermal=cells[11] == "Active",
            )
    return out


def read_game_log_events(path: Optional[str]) -> tuple[Counter, Counter, list[str]]:
    """Per second counts of PSO creation lines and of 'Tile Pool' / streaming lines, plus errors."""
    pso: Counter = Counter()
    streamer: Counter = Counter()
    errors: list[str] = []
    if not path or not os.path.exists(path):
        return pso, streamer, errors
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(r"\[\d{4}-\d{2}-\d{2} (\d{2}:\d{2}:\d{2})", line)
            if not m:
                continue
            key = m.group(1)
            low = line.lower()
            if "pso" in low and "never completed" not in low:
                pso[key] += 1
            if "tile pool" in low or "streamer" in low or "streaming" in low:
                streamer[key] += 1
            if "[error]" in low or "[critical]" in low:
                errors.append(line.rstrip()[:160])
    return pso, streamer, errors


def read_hitches(path: str) -> list[tuple[str, float, str]]:
    out: list[tuple[str, float, str]] = []
    if not os.path.exists(path):
        return out
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(r"\[(\d{2}:\d{2}:\d{2})\.\d+\] \[hitch\] ([0-9.]+) ms frame.*\| (.*)$", line)
            if m:
                out.append((m.group(1), float(m.group(2)), m.group(3).strip()))
    return out


def percentile(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    s = sorted(values)
    k = min(len(s) - 1, max(0, int(round(p / 100.0 * (len(s) - 1)))))
    return s[k]


def clusters(rows: list[dict[str, str]], slow_fps: float) -> list[list[dict[str, str]]]:
    """Consecutive seconds with fps below the threshold (ignoring seconds with no frames)."""
    out: list[list[dict[str, str]]] = []
    current: list[dict[str, str]] = []
    for row in rows:
        fps = float(row["fps"])
        if 0 < fps < slow_fps:
            current.append(row)
            continue
        if len(current) >= 2:
            out.append(current)
        current = []
    if len(current) >= 2:
        out.append(current)
    return out


def mean(values: list[float]) -> float:
    return statistics.fmean(values) if values else 0.0


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("session")
    ap.add_argument("--slow-fps", type=float, default=70.0)
    ap.add_argument("--from", dest="t_from", default="")
    ap.add_argument("--to", dest="t_to", default="")
    a = ap.parse_args()

    timeline = read_timeline(os.path.join(a.session, "acevo_perf_timeline.csv"))
    if a.t_from:
        timeline = [r for r in timeline if r["clock"] >= a.t_from]
    if a.t_to:
        timeline = [r for r in timeline if r["clock"] <= a.t_to]
    active = [r for r in timeline if int(r["frames"]) > 0]
    gpu = read_gpu(os.path.join(a.session, "gpu.csv"))
    game_logs = sorted(glob.glob(os.path.join(a.session, "log-*.txt")))
    pso, streamer, errors = read_game_log_events(game_logs[-1] if game_logs else None)
    hitches = read_hitches(os.path.join(a.session, "acevo_perf.log"))
    stamped = read_frames(os.path.join(a.session, "acevo_perf_frames.csv"))
    windowed = bool(a.t_from or a.t_to)
    if windowed and timeline:
        # a timeline row at t_s covers the frames presented in the second before it
        t_lo = min(float(r["t_s"]) for r in timeline) - 1.0
        t_hi = max(float(r["t_s"]) for r in timeline)
        stamped = [fr for fr in stamped if t_lo <= fr[0] <= t_hi]
        hitches = [h for h in hitches if (not a.t_from or h[0] >= a.t_from) and (not a.t_to or h[0] <= a.t_to)]
    frames = [fr[1] for fr in stamped]

    print(f"session: {a.session}")
    print(f"timeline: {len(timeline)} s total, {len(active)} s with frames, "
          f"{timeline[0]['clock'] if timeline else '?'} to {timeline[-1]['clock'] if timeline else '?'}")

    if frames:
        print(f"\n== frame times ({'presented frames in the window' if windowed else 'all presented frames'}) ==")
        print(f"frames {len(frames)}  mean {mean(frames):.2f} ms  ->  avg fps {1000 / mean(frames):.1f}")
        print(f"1% low {low_fps(frames, 0.01):.1f} fps   0.1% low {low_fps(frames, 0.001):.1f} fps   (mean of the slowest frames)")
        for p in (50, 90, 95, 99, 99.9):
            print(f"p{p:<5} {percentile(frames, p):7.2f} ms")
        for limit in (16.7, 20, 33, 50, 100):
            n = sum(1 for x in frames if x > limit)
            print(f">{limit:<5} ms: {n:6d} frames ({100.0 * n / len(frames):.2f} %)")
        print("\n== frame time spread ==")
        print_spread(stamped)
        print_gpu_split(os.path.join(a.session, "acevo_perf_gpu.csv"), stamped)
        print_sample_mix(os.path.join(a.session, "acevo_perf_samples.csv"),
                         min(fr[0] for fr in stamped), max(fr[0] for fr in stamped))

    fps_values = [float(r["fps"]) for r in active]
    print("\n== per second fps (seconds with frames) ==")
    print(f"mean {mean(fps_values):.1f}  min {min(fps_values) if fps_values else 0:.1f}  "
          f"seconds under {a.slow_fps:.0f} fps: {sum(1 for v in fps_values if v < a.slow_fps)}")

    print("\n== VRAM ==")
    used = [int(r["vram_used_mb"]) for r in active]
    budget = [int(r["vram_budget_mb"]) for r in active]
    if used:
        print(f"used max {max(used)} MB, mean {mean(used):.0f} MB, budget {max(budget)} MB, "
              f"margin at peak {max(budget) - max(used)} MB, seconds within 100 MB of budget: "
              f"{sum(1 for u, b in zip(used, budget) if b - u < 100)}")

    print("\n== streaming ==")
    tile_mb = sum(float(r["tile_mb"]) for r in timeline)
    f2m_mb = sum(float(r["f2m_mb"]) for r in timeline)
    gpumem_mb = sum(float(r["gpumem_mb"]) for r in timeline)
    tile_req = sum(int(r["tile_req"]) for r in timeline)
    batches = [int(r["tile_maxbatch"]) for r in timeline if int(r["tile_maxbatch"]) > 0]
    print(f"tiles {tile_req} requests / {tile_mb:.0f} MB, file->mem {f2m_mb:.0f} MB, mem->gpu {gpumem_mb:.0f} MB")
    if batches:
        print(f"largest tile batch per second: max {max(batches)}, median {statistics.median(batches):.0f}, "
              f"seconds with tile traffic {len(batches)}")
    busiest = sorted(timeline, key=lambda r: -float(r["tile_mb"]))[:5]
    print("busiest tile seconds: " + ", ".join(f"{r['clock']} {float(r['tile_mb']):.0f} MB/{r['tile_req']} req fps {float(r['fps']):.0f}" for r in busiest))

    print("\n== CPU ==")
    proc = [float(r["cpu_proc_pct"]) for r in active]
    sysp = [float(r["cpu_sys_pct"]) for r in active]
    if proc:
        print(f"game process mean {mean(proc):.1f} % of all cores (max {max(proc):.1f}), system mean {mean(sysp):.1f} % (max {max(sysp):.1f})")

    if gpu:
        print("\n== GPU (nvidia-smi) ==")
        samples = [gpu[r["clock"]] for r in active if r["clock"] in gpu]
        if samples:
            print(f"util mean {mean([s.util for s in samples]):.0f} %, clock mean {mean([s.clock for s in samples]):.0f} MHz "
                  f"(min {min(s.clock for s in samples)}, max {max(s.clock for s in samples)}), power mean {mean([s.power for s in samples]):.0f} W "
                  f"(max {max(s.power for s in samples):.0f}), temp max {max(s.temp for s in samples)} C")
            n = len(samples)
            print(f"throttle reasons active: sw thermal {100 * sum(s.sw_thermal for s in samples) / n:.0f} % of seconds, "
                  f"sw power cap {100 * sum(s.sw_power for s in samples) / n:.0f} %, hw thermal {100 * sum(s.hw_thermal for s in samples) / n:.0f} %")
            pairs = [(float(r["fps"]), gpu[r["clock"]].clock) for r in active if r["clock"] in gpu]
            if len(pairs) > 3:
                corr = statistics.correlation([p[0] for p in pairs], [p[1] for p in pairs])
                print(f"correlation fps vs GPU clock: {corr:+.2f}")

    print(f"\n== slow clusters (>= 2 consecutive seconds under {a.slow_fps:.0f} fps) ==")
    for c in clusters(active, a.slow_fps):
        keys = [r["clock"] for r in c]
        g = [gpu[k] for k in keys if k in gpu]
        gtxt = (f"gpu util {mean([s.util for s in g]):.0f}% clock {mean([s.clock for s in g]):.0f} MHz {mean([s.power for s in g]):.0f} W "
                f"{max(s.temp for s in g)} C thermal={'Y' if any(s.sw_thermal for s in g) else 'n'}") if g else "gpu n/a"
        print(f"{keys[0]}..{keys[-1]} ({len(c)} s) fps {mean([float(r['fps']) for r in c]):.0f} "
              f"max_ms {max(float(r['max_ms']) for r in c):.0f} | tiles {sum(float(r['tile_mb']) for r in c):.0f} MB "
              f"f2m {sum(float(r['f2m_mb']) for r in c):.0f} MB gpumem {sum(float(r['gpumem_mb']) for r in c):.0f} MB | "
              f"vram {max(int(r['vram_used_mb']) for r in c)}/{c[0]['vram_budget_mb']} | cpu {mean([float(r['cpu_proc_pct']) for r in c]):.0f}% | "
              f"pso {sum(pso[k] for k in keys)} | {gtxt}")

    if hitches:
        print(f"\n== hitches logged ({len(hitches)}, frames over the ini threshold) ==")
        by_sec: Counter = Counter(h[0] for h in hitches)
        print("seconds with most hitches: " + ", ".join(f"{k} x{v}" for k, v in by_sec.most_common(8)))
        for clock_key, ms, detail in sorted(hitches, key=lambda h: -h[1])[:10]:
            print(f"  {clock_key} {ms:7.1f} ms  {detail}")

    if pso:
        print(f"\n== PSO creations in the game log: {sum(pso.values())} lines, busiest seconds: "
              + ", ".join(f"{k} x{v}" for k, v in pso.most_common(6)))
    if errors:
        print(f"\n== game log errors ({len(errors)}) ==")
        for e in errors[:15]:
            print("  " + e)


if __name__ == "__main__":
    main()
