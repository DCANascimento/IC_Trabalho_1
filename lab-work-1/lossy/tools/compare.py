#!/usr/bin/env python3
"""Compares rate-distortion curves measured by bench.sh (Bjontegaard-style).

Usage:  tools/compare.py base.csv other.csv [other2.csv ...] [--per-step] [--per-file]

Each CSV has one row per file and step (columns file, step, kbps, snr_db, ...). For
each file, the points (SNR, log kbps) form a curve. Two curves are compared over
the SNR range they share:

  BD-rate  mean change of the bitrate at equal SNR, in %   (negative = fewer bits)
  BD-SNR   mean change of the SNR at equal bitrate, in dB  (positive = better)

--per-step  also prints, for each step of the reference, the change of bitrate
            needed to reach the same SNR as the reference at that step.
--per-file  also prints the BD values of each file.

Between points the curves are interpolated linearly (the original Bjontegaard
method fits a cubic; with many points both give nearly the same result).
Values are means over the files present in both CSVs.
Only the Python standard library is used.
"""

import csv
import math
import sys


def read_rows(path):
    """[(file, step, snr, log_kbps)] (infinite SNR is skipped)."""
    rows = []
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            snr = float(row["snr_db"])
            if math.isfinite(snr):
                rows.append((row["file"], float(row["step"]), snr, math.log(float(row["kbps"]))))
    return rows


def curves_of(rows):
    """{file: [(snr, log_kbps), ...]} sorted by SNR."""
    curves = {}
    for name, _, snr, log_kbps in rows:
        curves.setdefault(name, []).append((snr, log_kbps))
    for points in curves.values():
        points.sort()
    return curves


def interpolate(points, x):
    """Piecewise-linear y(x), or None if x is outside the range of the points."""
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        if x0 <= x <= x1:
            return y0 if x1 == x0 else y0 + (y1 - y0) * (x - x0) / (x1 - x0)
    return None


def mean_difference(a, b):
    """Mean of b(x) - a(x) over the x range shared by both curves (exact integral)."""
    low = max(a[0][0], b[0][0])
    high = min(a[-1][0], b[-1][0])
    if high <= low:
        return None
    xs = sorted({low, high} | {x for x, _ in a + b if low < x < high})
    area = 0.0
    for x0, x1 in zip(xs, xs[1:]):
        d0 = interpolate(b, x0) - interpolate(a, x0)
        d1 = interpolate(b, x1) - interpolate(a, x1)
        area += (d0 + d1) / 2 * (x1 - x0)
    return area / (high - low)


def bd(base, other):
    """{file: (bd_rate_percent, bd_snr_db)}"""
    swap = lambda points: sorted((y, x) for x, y in points)
    result = {}
    for name in sorted(base.keys() & other.keys()):
        a, b = base[name], other[name]
        log_rate = mean_difference(a, b)            # log kbps as a function of SNR
        snr = mean_difference(swap(a), swap(b))     # SNR as a function of log kbps
        if log_rate is not None and snr is not None:
            result[name] = (100 * (math.exp(log_rate) - 1), snr)
    return result


def per_step(base_rows, other):
    """{step: (mean SNR, mean kbps of the reference, mean bitrate change in %)}"""
    groups = {}
    for name, step, snr, log_kbps in base_rows:
        g = groups.setdefault(step, {"snr": [], "kbps": [], "delta": []})
        g["snr"].append(snr)
        g["kbps"].append(math.exp(log_kbps))
        y = interpolate(other.get(name, []), snr)
        if y is not None:
            g["delta"].append(100 * (math.exp(y - log_kbps) - 1))
    mean = lambda v: sum(v) / len(v) if v else None
    return {s: (mean(g["snr"]), mean(g["kbps"]), mean(g["delta"])) for s, g in groups.items()}


def main():
    paths = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(paths) < 2:
        print(__doc__.strip().splitlines()[2], file=sys.stderr)
        sys.exit(1)

    base_rows = read_rows(paths[0])
    base = curves_of(base_rows)
    others = [(p, curves_of(read_rows(p))) for p in paths[1:]]

    print(f"Reference: {paths[0]}\n")
    print("| compared with | BD-rate (%) | BD-SNR (dB) |")
    print("|---|---:|---:|")
    results = []
    for path, curves in others:
        r = bd(base, curves)
        results.append((path, r))
        if r:
            rate = sum(v[0] for v in r.values()) / len(r)
            snr = sum(v[1] for v in r.values()) / len(r)
            print(f"| {path} | {rate:+.2f} | {snr:+.3f} |")
        else:
            print(f"| {path} | no common SNR range | |")

    if "--per-step" in sys.argv:
        tables = [per_step(base_rows, curves) for _, curves in others]
        print("\nBitrate change at the same SNR as the reference (%; - = fewer bits)\n")
        print("| step | SNR (dB) | kbps | " + " | ".join(p for p, _ in others) + " |")
        print("|---:|---:|---:|" + "---:|" * len(others))
        for step in sorted(tables[0]):
            snr, kbps, _ = tables[0][step]
            cells = []
            for t in tables:
                delta = t[step][2]
                cells.append("—" if delta is None else f"{delta:+.1f}")
            print(f"| {step:g} | {snr:.2f} | {kbps:.1f} | " + " | ".join(cells) + " |")

    if "--per-file" in sys.argv:
        for path, r in results:
            print(f"\n{path}\n")
            print("| file | BD-rate (%) | BD-SNR (dB) |")
            print("|---|---:|---:|")
            for name, (rate, snr) in r.items():
                print(f"| {name} | {rate:+.2f} | {snr:+.3f} |")


if __name__ == "__main__":
    main()
