#!/usr/bin/env python3
"""telemetry_report.py - summarise one game session recorded by ACEvoPerf.

Reads from a session folder (see .agent/docs/ops/telemetry.md):
  acevo_perf_timeline.csv, acevo_perf_frames.csv, acevo_perf.log, gpu.csv (nvidia-smi sampler)
  and the game log (log-*.txt). All files are optional except the timeline.

A CSV the mod's log says it could not create this run is an older run's copy, so the report
refuses the timeline and leaves the frames out rather than summarise the wrong session.

Usage: telemetry_report.py SESSION_DIR [--slow-fps N] [--from HH:MM:SS] [--to HH:MM:SS]
"""
import argparse
import csv
import glob
import os
import re
import statistics
from collections import Counter
from dataclasses import dataclass
from typing import Optional

Frame = tuple[float, float, int, int]
Row = dict[str, str]


@dataclass
class GpuSample:
    util: int
    clock: int
    power: float
    temp: int
    sw_thermal: bool
    sw_power: bool
    hw_thermal: bool


def read_timeline(path: str) -> list[Row]:
    with open(path, newline="") as timeline:
        return list(csv.DictReader(timeline))


def read_frames(path: str) -> list[Frame]:
    """(seconds since attach, frame ms, tile requests, GPU uploads) per presented frame."""
    if not os.path.exists(path):
        return []
    with open(path, newline="") as frames:
        return [(float(row["t_s"]), float(row["frame_ms"]), int(row.get("tile_req") or 0), int(row.get("gpumem_req") or 0))
                for row in csv.DictReader(frames)]


def unwritten_csvs(log_path: str) -> set[str]:
    """The CSVs the mod's log says it could not create this run."""
    names: set[str] = set()
    if not os.path.exists(log_path):
        return names
    with open(log_path, encoding="utf-8", errors="replace") as log:
        for line in log:
            match = re.search(r"timeline: could not create (\S+) \(error", line)
            if match:
                names.add(match.group(1).lower())
    return names


def print_spread(stamped: list[Frame]) -> None:
    """How wide the frame time distribution is and whether the slow frames carry streaming."""
    frame_ms = [frame[1] for frame in stamped]
    median = statistics.median(frame_ms)
    total = sum(frame_ms)
    print(f"median {median:.2f} ms, p99/median {percentile(frame_ms, 99) / median:.2f}")
    for factor in (1.3, 1.5, 2.0):
        slow = [frame for frame in stamped if frame[1] > factor * median]
        above = sum(frame[1] - median for frame in slow)
        print(f"frames over {factor}x median ({factor * median:.1f} ms): {len(slow):5d} ({100.0 * len(slow) / len(frame_ms):.2f} %), "
              f"time above the median {above:.0f} ms ({100.0 * above / total:.1f} % of the window)")
    if any(frame[2] or frame[3] for frame in stamped):
        one_percent = max(1, len(stamped) // 100)
        slowest = sorted(stamped, key=lambda frame: -frame[1])[:one_percent]
        all_tiles = sum(1 for frame in stamped if frame[2] > 0)
        slow_tiles = sum(1 for frame in slowest if frame[2] > 0)
        slow_uploads = sum(1 for frame in slowest if frame[3] > 0)
        print(f"slowest 1% ({one_percent} frames): {slow_tiles} with tile requests, {slow_uploads} with GPU uploads, "
              f"against {100.0 * all_tiles / len(stamped):.1f} % of all frames with tile requests")


def low_fps(values: list[float], fraction: float) -> float:
    """The '1% low' style number: mean of the slowest fraction of frames, as fps."""
    if not values:
        return 0.0
    slowest_first = sorted(values, reverse=True)
    count = max(1, int(len(slowest_first) * fraction))
    return 1000.0 / mean(slowest_first[:count])


def digits(cell: str) -> int:
    return int(re.sub(r"[^0-9]", "", cell) or 0)


def read_gpu(path: str) -> dict[str, GpuSample]:
    """nvidia-smi csv keyed by HH:MM:SS (last sample of each second wins)."""
    samples: dict[str, GpuSample] = {}
    if not os.path.exists(path):
        return samples
    with open(path, newline="") as gpu_csv:
        for row in csv.reader(gpu_csv):
            if len(row) < 14 or not row[0].strip()[0:4].isdigit():
                continue
            cells = [cell.strip() for cell in row]
            clock_key = cells[0].split(" ")[1][:8]
            samples[clock_key] = GpuSample(
                util=digits(cells[1]),
                clock=digits(cells[4]),
                power=float(re.sub(r"[^0-9.]", "", cells[6]) or 0),
                temp=digits(cells[7]),
                sw_thermal=cells[12] == "Active",
                sw_power=cells[10] == "Active",
                hw_thermal=cells[11] == "Active",
            )
    return samples


def read_game_log_events(path: Optional[str]) -> tuple[Counter, list[str]]:
    """Per second counts of PSO creation lines, plus errors."""
    pso: Counter = Counter()
    errors: list[str] = []
    if not path or not os.path.exists(path):
        return pso, errors
    with open(path, encoding="utf-8", errors="replace") as game_log:
        for line in game_log:
            match = re.match(r"\[\d{4}-\d{2}-\d{2} (\d{2}:\d{2}:\d{2})", line)
            if not match:
                continue
            lowered = line.lower()
            if "pso" in lowered and "never completed" not in lowered:
                pso[match.group(1)] += 1
            if "[error]" in lowered or "[critical]" in lowered:
                errors.append(line.rstrip()[:160])
    return pso, errors


def read_hitches(path: str) -> list[tuple[str, float, str]]:
    hitches: list[tuple[str, float, str]] = []
    if not os.path.exists(path):
        return hitches
    with open(path, encoding="utf-8", errors="replace") as log:
        for line in log:
            match = re.match(r"\[(\d{2}:\d{2}:\d{2})\.\d+\] \[hitch\] ([0-9.]+) ms frame.*\| (.*)$", line)
            if match:
                hitches.append((match.group(1), float(match.group(2)), match.group(3).strip()))
    return hitches


def percentile(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round(p / 100.0 * (len(ordered) - 1)))))
    return ordered[index]


def clusters(rows: list[Row], slow_fps: float) -> list[list[Row]]:
    """Consecutive seconds with fps below the threshold (ignoring seconds with no frames)."""
    found: list[list[Row]] = []
    current: list[Row] = []
    for row in rows:
        fps = float(row["fps"])
        if 0 < fps < slow_fps:
            current.append(row)
            continue
        if len(current) >= 2:
            found.append(current)
        current = []
    if len(current) >= 2:
        found.append(current)
    return found


def mean(values: list[float]) -> float:
    return statistics.fmean(values) if values else 0.0


def correlation(xs: list[float], ys: list[float]) -> Optional[float]:
    """None when either side never moves, a pinned clock or a capped frame rate, where it has no meaning."""
    if len(set(xs)) < 2 or len(set(ys)) < 2:
        return None
    return statistics.correlation(xs, ys)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("session")
    parser.add_argument("--slow-fps", type=float, default=70.0)
    parser.add_argument("--from", dest="t_from", default="")
    parser.add_argument("--to", dest="t_to", default="")
    args = parser.parse_args()

    log_path = os.path.join(args.session, "acevo_perf.log")
    unwritten = unwritten_csvs(log_path)
    if "acevo_perf_timeline.csv" in unwritten:
        raise SystemExit("acevo_perf.log says the mod could not create acevo_perf_timeline.csv this run, "
                         "so the copy in this folder is an older run's and is not summarised")

    timeline = read_timeline(os.path.join(args.session, "acevo_perf_timeline.csv"))
    if args.t_from:
        timeline = [row for row in timeline if row["clock"] >= args.t_from]
    if args.t_to:
        timeline = [row for row in timeline if row["clock"] <= args.t_to]
    active = [row for row in timeline if int(row["frames"]) > 0]
    gpu = read_gpu(os.path.join(args.session, "gpu.csv"))
    game_logs = sorted(glob.glob(os.path.join(args.session, "log-*.txt")))
    pso, errors = read_game_log_events(game_logs[-1] if game_logs else None)
    hitches = read_hitches(log_path)
    stamped: list[Frame] = []
    if "acevo_perf_frames.csv" in unwritten:
        print("acevo_perf.log says the mod could not create acevo_perf_frames.csv this run, the frames are left out")
    else:
        stamped = read_frames(os.path.join(args.session, "acevo_perf_frames.csv"))
    windowed = bool(args.t_from or args.t_to)
    if windowed and timeline:
        # a timeline row at t_s covers the frames presented in the second before it
        t_lo = min(float(row["t_s"]) for row in timeline) - 1.0
        t_hi = max(float(row["t_s"]) for row in timeline)
        stamped = [frame for frame in stamped if t_lo <= frame[0] <= t_hi]
        hitches = [hitch for hitch in hitches if (not args.t_from or hitch[0] >= args.t_from) and (not args.t_to or hitch[0] <= args.t_to)]
    frames = [frame[1] for frame in stamped]

    print(f"session: {args.session}")
    print(f"timeline: {len(timeline)} s total, {len(active)} s with frames, "
          f"{timeline[0]['clock'] if timeline else '?'} to {timeline[-1]['clock'] if timeline else '?'}")

    if frames:
        print(f"\n== frame times ({'presented frames in the window' if windowed else 'all presented frames'}) ==")
        print(f"frames {len(frames)}  mean {mean(frames):.2f} ms  ->  avg fps {1000 / mean(frames):.1f}")
        print(f"1% low {low_fps(frames, 0.01):.1f} fps   0.1% low {low_fps(frames, 0.001):.1f} fps   (mean of the slowest frames)")
        for p in (50, 90, 95, 99, 99.9):
            print(f"p{p:<5} {percentile(frames, p):7.2f} ms")
        for limit in (16.7, 20, 33, 50, 100):
            over = sum(1 for frame_ms in frames if frame_ms > limit)
            print(f">{limit:<5} ms: {over:6d} frames ({100.0 * over / len(frames):.2f} %)")
        print("\n== frame time spread ==")
        print_spread(stamped)

    fps_values = [float(row["fps"]) for row in active]
    print("\n== per second fps (seconds with frames) ==")
    print(f"mean {mean(fps_values):.1f}  min {min(fps_values) if fps_values else 0:.1f}  "
          f"seconds under {args.slow_fps:.0f} fps: {sum(1 for fps in fps_values if fps < args.slow_fps)}")

    print("\n== VRAM ==")
    used = [int(row["vram_used_mb"]) for row in active]
    budget = [int(row["vram_budget_mb"]) for row in active]
    if used:
        print(f"used max {max(used)} MB, mean {mean(used):.0f} MB, budget {max(budget)} MB, "
              f"margin at peak {max(budget) - max(used)} MB, seconds within 100 MB of budget: "
              f"{sum(1 for used_mb, budget_mb in zip(used, budget) if budget_mb - used_mb < 100)}")

    print("\n== streaming ==")
    tile_mb = sum(float(row["tile_mb"]) for row in timeline)
    f2m_mb = sum(float(row["f2m_mb"]) for row in timeline)
    gpumem_mb = sum(float(row["gpumem_mb"]) for row in timeline)
    tile_req = sum(int(row["tile_req"]) for row in timeline)
    batches = [int(row["tile_maxbatch"]) for row in timeline if int(row["tile_maxbatch"]) > 0]
    print(f"tiles {tile_req} requests / {tile_mb:.0f} MB, file->mem {f2m_mb:.0f} MB, mem->gpu {gpumem_mb:.0f} MB")
    if batches:
        print(f"largest tile batch per second: max {max(batches)}, median {statistics.median(batches):.0f}, "
              f"seconds with tile traffic {len(batches)}")
    busiest = sorted(timeline, key=lambda row: -float(row["tile_mb"]))[:5]
    print("busiest tile seconds: " + ", ".join(f"{row['clock']} {float(row['tile_mb']):.0f} MB/{row['tile_req']} req fps {float(row['fps']):.0f}" for row in busiest))

    print("\n== CPU ==")
    process_pct = [float(row["cpu_proc_pct"]) for row in active]
    system_pct = [float(row["cpu_sys_pct"]) for row in active]
    if process_pct:
        print(f"game process mean {mean(process_pct):.1f} % of all cores (max {max(process_pct):.1f}), "
              f"system mean {mean(system_pct):.1f} % (max {max(system_pct):.1f})")

    if gpu:
        print("\n== GPU (nvidia-smi) ==")
        samples = [gpu[row["clock"]] for row in active if row["clock"] in gpu]
        if samples:
            print(f"util mean {mean([sample.util for sample in samples]):.0f} %, clock mean {mean([sample.clock for sample in samples]):.0f} MHz "
                  f"(min {min(sample.clock for sample in samples)}, max {max(sample.clock for sample in samples)}), "
                  f"power mean {mean([sample.power for sample in samples]):.0f} W "
                  f"(max {max(sample.power for sample in samples):.0f}), temp max {max(sample.temp for sample in samples)} C")
            count = len(samples)
            print(f"throttle reasons active: sw thermal {100 * sum(sample.sw_thermal for sample in samples) / count:.0f} % of seconds, "
                  f"sw power cap {100 * sum(sample.sw_power for sample in samples) / count:.0f} %, "
                  f"hw thermal {100 * sum(sample.hw_thermal for sample in samples) / count:.0f} %")
            pairs = [(float(row["fps"]), gpu[row["clock"]].clock) for row in active if row["clock"] in gpu]
            if len(pairs) > 3:
                fps_clock = correlation([pair[0] for pair in pairs], [float(pair[1]) for pair in pairs])
                if fps_clock is None:
                    print("correlation fps vs GPU clock: none, one of them never moved")
                else:
                    print(f"correlation fps vs GPU clock: {fps_clock:+.2f}")

    print(f"\n== slow clusters (>= 2 consecutive seconds under {args.slow_fps:.0f} fps) ==")
    for cluster in clusters(active, args.slow_fps):
        keys = [row["clock"] for row in cluster]
        cluster_gpu = [gpu[key] for key in keys if key in gpu]
        gpu_text = (f"gpu util {mean([sample.util for sample in cluster_gpu]):.0f}% clock {mean([sample.clock for sample in cluster_gpu]):.0f} MHz "
                    f"{mean([sample.power for sample in cluster_gpu]):.0f} W {max(sample.temp for sample in cluster_gpu)} C "
                    f"thermal={'Y' if any(sample.sw_thermal for sample in cluster_gpu) else 'n'}") if cluster_gpu else "gpu n/a"
        print(f"{keys[0]}..{keys[-1]} ({len(cluster)} s) fps {mean([float(row['fps']) for row in cluster]):.0f} "
              f"max_ms {max(float(row['max_ms']) for row in cluster):.0f} | tiles {sum(float(row['tile_mb']) for row in cluster):.0f} MB "
              f"f2m {sum(float(row['f2m_mb']) for row in cluster):.0f} MB gpumem {sum(float(row['gpumem_mb']) for row in cluster):.0f} MB | "
              f"vram {max(int(row['vram_used_mb']) for row in cluster)}/{cluster[0]['vram_budget_mb']} | "
              f"cpu {mean([float(row['cpu_proc_pct']) for row in cluster]):.0f}% | "
              f"pso {sum(pso[key] for key in keys)} | {gpu_text}")

    if hitches:
        print(f"\n== hitches logged ({len(hitches)}, frames over the ini threshold) ==")
        by_second: Counter = Counter(hitch[0] for hitch in hitches)
        print("seconds with most hitches: " + ", ".join(f"{second} x{count}" for second, count in by_second.most_common(8)))
        for clock_key, frame_ms, detail in sorted(hitches, key=lambda hitch: -hitch[1])[:10]:
            print(f"  {clock_key} {frame_ms:7.1f} ms  {detail}")

    if pso:
        print(f"\n== PSO creations in the game log: {sum(pso.values())} lines, busiest seconds: "
              + ", ".join(f"{second} x{count}" for second, count in pso.most_common(6)))
    if errors:
        print(f"\n== game log errors ({len(errors)}) ==")
        for error in errors[:15]:
            print("  " + error)


if __name__ == "__main__":
    main()
