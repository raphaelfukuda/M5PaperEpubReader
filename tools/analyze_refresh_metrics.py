#!/usr/bin/env python3
"""Summarize structured M5EPUB refresh metrics from a serial log."""

from __future__ import annotations

import argparse
import csv
import statistics
import sys
from collections import defaultdict
from pathlib import Path
from typing import Iterable, TextIO

PREFIX = "M5EPUB_METRIC,"
DEFAULT_FIELDS = (
    "event_to_handling_us",
    "event_to_page_ready_us",
    "canvas_render_us",
    "display_wait_us",
    "sprite_upload_us",
    "display_command_us",
    "panel_busy_us",
    "cpu_work_during_busy_us",
    "event_to_submit_us",
    "event_to_idle_us",
    "updated_pixels",
    "total_us",
)


def parse_metric_line(line: str) -> dict[str, str] | None:
    marker = line.find(PREFIX)
    if marker < 0:
        return None
    fields: dict[str, str] = {}
    for item in line[marker + len(PREFIX) :].strip().split(","):
        if "=" not in item:
            continue
        key, value = item.split("=", 1)
        fields[key.strip()] = value.strip()
    return fields if fields.get("type") == "page_turn" else None


def percentile(values: list[int], percent: float) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return float(ordered[0])
    position = (len(ordered) - 1) * percent
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    fraction = position - lower
    return ordered[lower] + (ordered[upper] - ordered[lower]) * fraction


def summarize(lines: Iterable[str]) -> list[dict[str, object]]:
    groups: dict[tuple[str, str, str], list[dict[str, str]]] = defaultdict(list)
    for line in lines:
        metric = parse_metric_line(line)
        if metric is None:
            continue
        key = (metric.get("mode", "unknown"), metric.get("kind", "unknown"),
               metric.get("region", "unknown"))
        groups[key].append(metric)

    rows: list[dict[str, object]] = []
    for (mode, kind, region), samples in sorted(groups.items()):
        for field in DEFAULT_FIELDS:
            values = [int(sample[field]) for sample in samples
                      if sample.get(field, "").isdigit()]
            if not values:
                continue
            rows.append({
                "mode": mode, "kind": kind, "region": region, "metric": field,
                "count": len(values), "min": min(values),
                "mean": statistics.fmean(values), "median": statistics.median(values),
                "p90": percentile(values, 0.90), "p95": percentile(values, 0.95),
                "max": max(values),
            })
    return rows


def write_markdown(rows: list[dict[str, object]], output: TextIO) -> None:
    columns = ("mode", "kind", "region", "metric", "count", "min", "mean",
               "median", "p90", "p95", "max")
    output.write("| " + " | ".join(columns) + " |\n")
    output.write("|" + "|".join("---" for _ in columns) + "|\n")
    for row in rows:
        values = []
        for column in columns:
            value = row[column]
            values.append(f"{value:.2f}" if isinstance(value, float) else str(value))
        output.write("| " + " | ".join(values) + " |\n")


def write_csv(rows: list[dict[str, object]], output: TextIO) -> None:
    if not rows:
        return
    writer = csv.DictWriter(output, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path, help="serial log file")
    parser.add_argument("--format", choices=("markdown", "csv"), default="markdown")
    parser.add_argument("-o", "--output", type=Path)
    args = parser.parse_args()
    with args.log.open(encoding="utf-8", errors="replace") as log_file:
        rows = summarize(log_file)
    output = args.output.open("w", encoding="utf-8", newline="") if args.output else sys.stdout
    try:
        (write_csv if args.format == "csv" else write_markdown)(rows, output)
    finally:
        if args.output:
            output.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
