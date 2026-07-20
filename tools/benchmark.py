#!/usr/bin/env python3
"""Reproducible DSPSeedCalc benchmark runner."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import platform
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

try:
    import psutil
except ImportError:
    psutil = None


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def median_mad(values: list[float]) -> tuple[float, float]:
    median = statistics.median(values)
    return median, statistics.median(abs(value - median) for value in values)


def seed_count(range_text: str) -> int:
    seed_part = range_text.split(",", 1)[0]
    if "-" not in seed_part:
        return 1
    start, end = seed_part.split("-", 1)
    return int(end) - int(start) + 1


def run_once(command: list[str], cwd: pathlib.Path) -> dict:
    start = time.perf_counter_ns()
    process = subprocess.Popen(command, cwd=cwd, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    peak_rss = 0
    user_cpu = 0.0
    system_cpu = 0.0
    if psutil:
        child = psutil.Process(process.pid)
        while process.poll() is None:
            try:
                peak_rss = max(peak_rss, child.memory_info().rss)
                times = child.cpu_times()
                user_cpu = max(user_cpu, times.user)
                system_cpu = max(system_cpu, times.system)
            except psutil.Error:
                pass
            time.sleep(0.01)
    stdout, stderr = process.communicate()
    end = time.perf_counter_ns()
    output_bytes = sum(path.stat().st_size for path in cwd.iterdir() if path.is_file())
    return {
        "returncode": process.returncode,
        "wallSeconds": (end - start) / 1_000_000_000,
        "peakRssBytes": peak_rss,
        "userCpuSeconds": user_cpu,
        "systemCpuSeconds": system_cpu,
        "stdoutBytes": len(stdout),
        "stderrBytes": len(stderr),
        "outputBytes": output_bytes,
        "stderr": stderr.decode(errors="replace"),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=pathlib.Path)
    parser.add_argument("--plugin-dir", type=pathlib.Path, required=True)
    parser.add_argument("--prototypes", type=pathlib.Path,
                        default=pathlib.Path("Prototypes"))
    parser.add_argument("--manifest", type=pathlib.Path,
                        default=pathlib.Path("benchmarks/workloads.json"))
    parser.add_argument("--tier", default="smoke")
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument("--result", type=pathlib.Path,
                        default=pathlib.Path("benchmark-results.json"))
    parser.add_argument("--compiler", default="unknown")
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    cases = manifest["tiers"][args.tier]
    expanded = []
    for case in cases:
        if "range" in case:
            expanded.append(case)
        else:
            for stars in case["starCounts"]:
                for threads in case["threads"]:
                    item = dict(case)
                    item.update(name=f"{case['name']}-{stars}-t{threads}",
                                range=f"1-100,{stars}", threads=threads)
                    expanded.append(item)

    report = {
        "manifestVersion": manifest["version"],
        "executable": str(args.executable.resolve()),
        "executableSha256": sha256(args.executable),
        "platform": sys.platform,
        "platformDetail": platform.platform(),
        "processor": platform.processor(),
        "cpuCount": os.cpu_count(),
        "compiler": args.compiler,
        "workingDirectory": str(pathlib.Path.cwd()),
        "prototypeSha256": {
            path.name: sha256(path) for path in sorted(args.prototypes.glob("*.json"))
            if path.name in {"ThemeProtoSet.json", "ItemProtoSet.json",
                             "VeinProtoSet.json", "StringProtoSet.json"}
        },
        "cases": [],
    }
    temp_root = pathlib.Path(tempfile.mkdtemp(prefix="dsp-benchmark-"))
    try:
        for case in expanded:
            samples = []
            for run in range(args.runs + 1):
                run_dir = temp_root / case["name"] / str(run)
                run_dir.mkdir(parents=True)
                shutil.copytree(args.prototypes, run_dir / "Prototypes")
                filters = run_dir / "filters"
                filters.mkdir()
                if case.get("plugin"):
                    plugin = args.plugin_dir / f"{case['plugin']}.dll"
                    shutil.copy2(plugin, filters / plugin.name)
                command = [str(args.executable.resolve()), "-t", str(case["threads"]),
                           "-o", "seeds.csv", *case["mode"], case["range"]]
                sample = run_once(command, run_dir)
                if sample["returncode"]:
                    print(sample["stderr"], file=sys.stderr)
                    return sample["returncode"]
                if run:
                    samples.append(sample)
            wall_median, wall_mad = median_mad([sample["wallSeconds"] for sample in samples])
            report["cases"].append({
                "case": case,
                "samples": samples,
                "wallMedianSeconds": wall_median,
                "wallMadSeconds": wall_mad,
                "processedSeeds": seed_count(case["range"]),
                "seedsPerSecond": seed_count(case["range"]) / wall_median,
            })
    finally:
        shutil.rmtree(temp_root, ignore_errors=True)
    args.result.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"wrote {args.result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
