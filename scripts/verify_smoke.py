"""Validate fresh smoke results on either CI OS; never assert benchmark speed."""
from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--platform", choices=("Linux", "Windows"), required=True)
    args = parser.parse_args()
    reports = list(args.folder.glob("*/results.json"))
    require(len(reports) == 1, "Expected exactly one fresh smoke session")
    report = reports[0]
    data = json.loads(report.read_text(encoding="utf-8"))
    require(data["schema"] == "AM5Native/3", "Unexpected report schema")
    require(data["engine"] == "AM5-Native-2.1.0-AVX2", "Unexpected engine version")
    require(data["platform"].startswith(args.platform), "Wrong executable platform")
    require(data["complete"] is True, "Benchmark did not complete")
    require(data["selftests_passed"] == 60, "Not all 60 self-tests passed")
    require(data["integrity"]["errors"] == 0, "Data verification failed")
    require(data["options"]["smoke"] is True, "Not a smoke run")
    require(data["sufficient_memory"] is False, "Smoke results must not be labelled DRAM scores")
    require(data["diagnostics"] == [], "Smoke mode must not emit tuning priorities")
    require(data["profile_loaded"] is True, "Packaged profile could not be read")
    require(data["machine"]["avx2"] is True, "AVX2 support missing")
    require(data["timer_frequency_hz"] > 0, "Invalid timer frequency")
    samples = data["samples"]
    require(bool(samples), "No samples recorded")
    turn_groups = set()
    stall_count = 0
    for sample in samples:
        label = f'{sample["suite"]}/{sample["test"]}'
        require(sample["errors"] == 0, f"Sample integrity error: {label}")
        require(sample["pin_ok"] is True, f"Thread affinity failed: {label}")
        for key in ("elapsed", "value"):
            require(math.isfinite(sample[key]) and sample[key] > 0, f"Invalid {key}: {label}")
        require(sample["pattern_bytes"] >= 0 and sample["events"] >= 0, f"Bad diagnostic metadata: {label}")
        if sample["unit"] == "GB/s":
            expected = sample["logical_bytes"] / sample["elapsed"] / 1e9
            if sample["suite"] == "turnaround_sweep":
                require(sample["pattern_bytes"] in {64, 256, 1024, 4096, 16384, 65536}, "Unexpected turnaround group")
                require(sample["events"] > 0, "Turnaround sample has no direction-switch events")
                turn_groups.add(sample["pattern_bytes"])
        else:
            require(sample["unit"] == "ns/access", f"Unknown unit: {label}")
            require(sample["operations"] > 0, f"No operations: {label}")
            expected = sample["elapsed"] * 1e9 / sample["operations"]
            require(0 < sample["p50"] <= sample["p95"] <= sample["p99"] <= sample["p999"] <= sample["max"], f"Bad quantiles: {label}")
            if sample["suite"] == "stall_probe":
                require(sample["pattern_bytes"] == 64, "Short-window probe must use 64 dependent accesses")
                stall_count += 1
        require(math.isclose(sample["value"], expected, rel_tol=1e-9), f"Timing/byte arithmetic mismatch: {label}")
    require(turn_groups == {64, 256, 1024, 4096, 16384, 65536}, "Turnaround sweep is incomplete")
    require(stall_count == data["options"]["repeats"], "Short-window stall probe repeat count mismatch")
    with report.with_name("raw.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    require(len(rows) == len(samples), "CSV/JSON sample counts differ")
    for row, sample in zip(rows, samples):
        require((row["suite"], row["test"], int(row["trial"])) ==
                (sample["suite"], sample["test"], sample["trial"]), "CSV/JSON identity mismatch")
        require(math.isclose(float(row["value"]), sample["value"], rel_tol=1e-8, abs_tol=1e-8),
                "CSV/JSON value mismatch")
    html = report.with_name("report.html").read_text(encoding="utf-8")
    require("__DATA__" not in html and "AM5Native/3" in html and "读写方向切换扫描" in html,
            "HTML data/report UI was not embedded")
    print(f"PASS: {args.platform}; 60 self-tests; {len(samples)} smoke samples; diagnostic suites and JSON/CSV/HTML verified.")
    print("Functional CI checks only: not a performance reference or overclock stability certificate.")


if __name__ == "__main__":
    main()
