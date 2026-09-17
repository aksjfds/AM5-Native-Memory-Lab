"""Validate a fresh Copy Analyzer smoke run on either CI OS."""
from __future__ import annotations

import argparse
import csv
import json
import math
from collections import Counter
from pathlib import Path

ENGINE = "AM5-Native-2.2.2-COPY-GROUPS-AVX2"
SELFTESTS = 65
TURNAROUND_PATTERNS = [64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 65536]
CSV_NINE_DECIMAL_ABS = 5.1e-10
CSV_SIX_DECIMAL_ABS = 5.1e-7


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def close(a: float, b: float, *, rel: float = 1e-8, abs_: float = 1e-8) -> bool:
    return math.isclose(a, b, rel_tol=rel, abs_tol=abs_)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--platform", choices=("Linux", "Windows"), required=True)
    args = parser.parse_args()

    reports = list(args.folder.glob("*/results.json"))
    require(len(reports) == 1, "Expected exactly one fresh smoke session")
    report = reports[0]
    data = json.loads(report.read_text(encoding="utf-8"))

    require(data["schema"] == "AM5Native/4", "Unexpected report schema")
    require(data.get("focus") == "copy_bottleneck", "Wrong analysis focus")
    require(data.get("diagnostic_scope") == "parameter_group_only", "Wrong diagnostic scope")
    require(data["engine"] == ENGINE, "Unexpected engine id")
    require(data["platform"].startswith(args.platform), "Wrong executable platform")
    require(data["complete"] is True, "Benchmark did not complete")
    require(data["selftests_passed"] == SELFTESTS, f"Not all {SELFTESTS} self-tests passed")
    require(data["integrity"]["errors"] == 0, "Data verification failed")
    require(data["options"]["smoke"] is True, "Not a smoke run")
    require(data["sufficient_memory"] is False, "Smoke results must not be labelled DRAM scores")
    require(data["profile_loaded"] is True, "Packaged profile could not be read")
    require(data["machine"]["avx2"] is True, "AVX2 support missing")
    require(data["timer_frequency_hz"] > 0, "Invalid timer frequency")
    require(data["diagnostics"] == [], "Smoke mode must suppress performance diagnosis")

    repeats = data["options"]["repeats"]
    threads = data["options"]["threads"]
    samples = data["samples"]
    require(bool(samples), "No samples recorded")

    for sample in samples:
        label = f'{sample["suite"]}/{sample["test"]}/trial={sample["trial"]}'
        require(sample["errors"] == 0, f"Sample integrity error: {label}")
        require(sample["pin_ok"] is True, f"Thread affinity failed: {label}")
        require(math.isfinite(sample["elapsed"]) and sample["elapsed"] > 0, f"Invalid elapsed: {label}")
        require(math.isfinite(sample["value"]) and sample["value"] > 0, f"Invalid value: {label}")
        if sample["unit"] == "GB/s":
            require(sample["logical_bytes"] > 0, f"No logical bytes: {label}")
            expected = sample["logical_bytes"] / sample["elapsed"] / 1e9
        else:
            require(sample["unit"] == "ns/access", f"Unknown unit: {label}")
            require(sample["operations"] > 0, f"No operations: {label}")
            expected = sample["elapsed"] * 1e9 / sample["operations"]
            require(0 < sample["p50"] <= sample["p95"] <= sample["p99"] <= sample["p999"] <= sample["max"],
                    f"Bad quantiles: {label}")
        require(close(sample["value"], expected, rel=1e-9, abs_=0), f"Timing/byte arithmetic mismatch: {label}")

    turnaround = [s for s in samples if s["suite"] == "copy_turnaround"]
    counts = Counter(s["pattern_bytes"] for s in turnaround)
    require(sorted(counts) == TURNAROUND_PATTERNS, f"Wrong turnaround sweep: {sorted(counts)}")
    require(all(counts[p] == repeats for p in TURNAROUND_PATTERNS), f"Incomplete turnaround repeats: {counts}")
    require(all(s["threads"] == threads and s["events"] > 0 for s in turnaround),
            "Turnaround samples have wrong thread count or zero events")

    expected_copy_threads = sorted({t for t in (1, 2, 4, 8, threads) if t <= threads})
    actual_copy_threads = sorted({s["threads"] for s in samples
                                  if s["suite"] == "copy_baseline" and s["test"] == "copy_nt"})
    require(actual_copy_threads == expected_copy_threads,
            f"Wrong Copy scaling points: expected {expected_copy_threads}, got {actual_copy_threads}")
    for test in ("read", "write_nt"):
        actual = sorted({s["threads"] for s in samples if s["suite"] == "copy_baseline" and s["test"] == test})
        require(actual == sorted({1, threads}), f"Wrong {test} reference threads: {actual}")
    require(sum(1 for s in samples if s["suite"] == "copy_baseline" and s["test"] == "copy_cached") == repeats,
            "Missing Cached Copy repeats")

    background_threads = threads - 1
    require(any(s["suite"] == "loaded_latency" and s["test"] == "idle" for s in samples),
            "Missing idle loaded-latency reference")
    if background_threads > 0:
        for test in ("read_load", "mixed_load", "copy_load"):
            require(any(s["suite"] == "loaded_latency" and s["test"] == test and s["threads"] == background_threads
                        for s in samples), f"Missing {test} at max background threads")
        require(any(s["suite"] == "copy_stall_probe" and s["test"] == "copy_load_short_window"
                    and s["threads"] == background_threads for s in samples),
                "Missing Copy-loaded short-window probe")
    require(any(s["suite"] == "copy_stall_probe" and s["test"] == "idle_short_window" for s in samples),
            "Missing idle short-window probe")

    csv_path = report.with_name("raw.csv")
    with csv_path.open(encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        rows = list(reader)
        fieldnames = set(reader.fieldnames or [])
    require(rows, "CSV is empty")
    required_fields = {
        "suite", "test", "trial", "threads", "chains", "working_bytes", "pattern_bytes",
        "operations", "events", "logical_bytes", "elapsed_s", "value", "unit",
        "batch_p50_ns", "batch_p95_ns", "batch_p99_ns", "batch_p999_ns", "batch_max_ns",
        "background_GBps", "affinity_ok", "errors",
    }
    require(required_fields.issubset(fieldnames), f"CSV fields missing: {sorted(required_fields - fieldnames)}")
    require(len(rows) == len(samples), "CSV/JSON sample counts differ")

    for row, sample in zip(rows, samples):
        label = f'{sample["suite"]}/{sample["test"]}/trial={sample["trial"]}'
        require((row["suite"], row["test"], int(row["trial"])) ==
                (sample["suite"], sample["test"], sample["trial"]), f"CSV/JSON identity mismatch: {label}")
        for key in ("threads", "chains", "working_bytes", "pattern_bytes", "operations", "events", "logical_bytes"):
            require(int(row[key]) == int(sample[key]), f"CSV/JSON {key} mismatch: {label}")
        require(row["unit"] == sample["unit"], f"CSV/JSON unit mismatch: {label}")
        require(int(row["affinity_ok"]) == int(sample["pin_ok"]), f"CSV/JSON affinity mismatch: {label}")
        require(int(row["errors"]) == int(sample["errors"]), f"CSV/JSON errors mismatch: {label}")
        require(close(float(row["elapsed_s"]), sample["elapsed"], abs_=CSV_NINE_DECIMAL_ABS),
                f"CSV/JSON elapsed mismatch: {label}")
        require(close(float(row["value"]), sample["value"], abs_=CSV_NINE_DECIMAL_ABS),
                f"CSV/JSON value mismatch: {label}")
        require(close(float(row["background_GBps"]), sample["background_gbps"], abs_=CSV_SIX_DECIMAL_ABS),
                f"CSV/JSON background mismatch: {label}")
        for csv_key, json_key in (("batch_p50_ns", "p50"), ("batch_p95_ns", "p95"),
                                  ("batch_p99_ns", "p99"), ("batch_p999_ns", "p999"),
                                  ("batch_max_ns", "max")):
            require(close(float(row[csv_key]), sample[json_key], abs_=CSV_SIX_DECIMAL_ABS),
                    f"CSV/JSON {json_key} mismatch: {label}")

    html = report.with_name("report.html").read_text(encoding="utf-8")
    require("__DATA__" not in html and "AM5Native/4" in html and ENGINE in html and
            '"diagnostic_scope":"parameter_group_only"' in html and "Copy 短板排名" in html,
            "HTML report did not embed the expected group-only v4 data")
    print(f"PASS: {args.platform}; {SELFTESTS} self-tests; {len(samples)} smoke samples; Copy group-only v4 JSON/CSV/HTML verified.")
    print("Functional CI checks only: not a performance reference or overclock stability certificate.")


if __name__ == "__main__":
    main()
