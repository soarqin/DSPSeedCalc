#!/usr/bin/env python3
"""Reproducible DSPSeedCalc benchmark runner."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import platform
import re
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


GENERATE_PATTERN = re.compile(r"(\d+)ms used, (\d+) found from (\d+) processed seeds")


def parse_generate(stderr: str) -> dict:
    """Extract the program's own timer, which excludes startup and Prototype parsing."""
    match = GENERATE_PATTERN.search(stderr)
    if not match:
        return {}
    return {
        "generateSeconds": int(match.group(1)) / 1000.0,
        "found": int(match.group(2)),
        "processedSeeds": int(match.group(3)),
    }


OPTIMIZED_BUILD_TYPES = {"release", "relwithdebinfo", "minsizerel"}


def read_build_config(executable: pathlib.Path) -> dict:
    """Find the CMakeCache.txt that produced `executable` and read the build settings.

    A build directory left with an empty CMAKE_BUILD_TYPE produces an unoptimized
    binary that still links and runs, which silently invalidates every number the
    runner reports. Recording the setting makes that visible in the result file.
    """
    wanted = {"CMAKE_BUILD_TYPE", "RELEASE_USE_LTO", "CMAKE_CXX_COMPILER",
              "DSP_PGO", "DSP_ENABLE_NATIVE_ARCH", "DSP_ENABLE_INSTRUMENTATION"}
    for directory in executable.resolve().parents:
        cache = directory / "CMakeCache.txt"
        if not cache.is_file():
            continue
        config = {}
        for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
            name, separator, value = line.partition(":")
            if not separator or "=" not in value:
                continue
            if name in wanted:
                config[name] = value.split("=", 1)[1]
        config["cmakeCache"] = str(cache)
        return config
    return {}


def plan_seeds(case: dict, stars: int, threads: int, scale: float) -> int:
    """Size a case so the program's own timer stays far above its 1 ms resolution.

    `seedsAt64` is the budget for 64 stars on one thread. Per-seed cost grows
    roughly linearly with the star count, and wall time shrinks roughly linearly
    with the thread count, so both are compensated to keep every case in the same
    duration band instead of letting process startup dominate the short ones.
    """
    base = case.get("seedsAt64", 5000) * 64.0 / max(1, stars) * max(1, threads)
    return max(case.get("minSeeds", 200), int(round(base * scale)))


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
    parser.add_argument("--seed-scale", type=float, default=1.0,
                        help="multiplier on the planned seed count; lower it for a quick pass")
    parser.add_argument("--allow-unoptimized", action="store_true",
                        help="run even when the build directory is not an optimized build")
    args = parser.parse_args()

    build_config = read_build_config(args.executable)
    build_type = build_config.get("CMAKE_BUILD_TYPE", "")
    if build_type.lower() not in OPTIMIZED_BUILD_TYPES:
        message = (f"build type {build_type!r} is not optimized "
                   f"(cache: {build_config.get('cmakeCache', 'not found')})")
        if not args.allow_unoptimized:
            print(f"refusing to benchmark: {message}\n"
                  f"configure with --preset benchmark, or pass --allow-unoptimized",
                  file=sys.stderr)
            return 2
        print(f"warning: {message}", file=sys.stderr)

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
                    seeds = plan_seeds(case, stars, threads, args.seed_scale)
                    item.update(name=f"{case['name']}-{stars}-t{threads}",
                                range=f"1-{seeds},{stars}", threads=threads)
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
        "buildConfig": build_config,
        "seedScale": args.seed_scale,
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
                sample.update(parse_generate(sample["stderr"]))
                if run:
                    samples.append(sample)
            wall_median, wall_mad = median_mad([sample["wallSeconds"] for sample in samples])
            seeds = seed_count(case["range"])
            entry = {
                "case": case,
                "samples": samples,
                "wallMedianSeconds": wall_median,
                "wallMadSeconds": wall_mad,
                "processedSeeds": seeds,
                "wallSeedsPerSecond": seeds / wall_median,
            }
            generate = [sample["generateSeconds"] for sample in samples
                        if "generateSeconds" in sample]
            if len(generate) == len(samples) and all(value > 0.0 for value in generate):
                generate_median, generate_mad = median_mad(generate)
                entry.update(generateMedianSeconds=generate_median,
                             generateMadSeconds=generate_mad,
                             seedsPerSecond=seeds / generate_median)
            else:
                # No usable internal timer: fall back to wall time and say so, rather
                # than silently reporting a number dominated by process startup.
                entry.update(generateMedianSeconds=None, generateMadSeconds=None,
                             seedsPerSecond=seeds / wall_median,
                             seedsPerSecondSource="wall")
            report["cases"].append(entry)
            print(f"{case['name']}: {entry['seedsPerSecond']:.0f} seeds/s "
                  f"({seeds} seeds, generate median "
                  f"{entry['generateMedianSeconds'] or wall_median:.3f}s)", flush=True)
    finally:
        shutil.rmtree(temp_root, ignore_errors=True)
    args.result.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"wrote {args.result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
