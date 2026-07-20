#!/usr/bin/env python3
"""Run the fixed PGO training manifest in isolated directories."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import subprocess
import tempfile


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=pathlib.Path)
    parser.add_argument("--plugin-dir", type=pathlib.Path, required=True)
    parser.add_argument("--prototypes", type=pathlib.Path,
                        default=pathlib.Path("Prototypes"))
    parser.add_argument("--manifest", type=pathlib.Path,
                        default=pathlib.Path("benchmarks/training-workloads.json"))
    parser.add_argument("--clang-profile-pattern")
    args = parser.parse_args()
    if args.clang_profile_pattern:
        profile_pattern = pathlib.Path(args.clang_profile_pattern)
        if not profile_pattern.is_absolute() or not any(
                token in args.clang_profile_pattern for token in ("%p", "%m")):
            parser.error("--clang-profile-pattern must be absolute and contain %p or %m")
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    root = pathlib.Path(tempfile.mkdtemp(prefix="dsp-pgo-"))
    try:
        for index, workload in enumerate(manifest["workloads"]):
            run_dir = root / str(index)
            run_dir.mkdir()
            shutil.copytree(args.prototypes, run_dir / "Prototypes")
            filters = run_dir / "filters"
            filters.mkdir()
            plugin = args.plugin_dir / f"{workload['plugin']}.dll"
            shutil.copy2(plugin, filters / plugin.name)
            command = [str(args.executable.resolve()), "-t", str(workload["threads"]),
                       "-o", "seeds.csv", *workload["mode"], workload["range"]]
            environment = os.environ.copy()
            if args.clang_profile_pattern:
                environment["LLVM_PROFILE_FILE"] = args.clang_profile_pattern
            result = subprocess.run(command, cwd=run_dir, env=environment)
            if result.returncode:
                return result.returncode
    finally:
        shutil.rmtree(root, ignore_errors=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
