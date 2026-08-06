#!/usr/bin/env python3
"""Run the precise vein amount oracle and compare or update its fixture."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=pathlib.Path)
    parser.add_argument(
        "--fixture",
        type=pathlib.Path,
        default=pathlib.Path("tests/fixtures/vein-amounts-v1.txt"),
    )
    parser.add_argument("--update", action="store_true")
    args = parser.parse_args()

    result = subprocess.run(
        [str(args.executable.resolve())],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=pathlib.Path.cwd(),
        check=False,
    )
    if result.returncode:
        sys.stderr.buffer.write(result.stderr)
        return result.returncode

    digest = hashlib.sha256(result.stdout).hexdigest()
    if args.update:
        args.fixture.parent.mkdir(parents=True, exist_ok=True)
        args.fixture.write_bytes(result.stdout)
        print(f"updated {args.fixture} sha256={digest}")
        return 0
    if not args.fixture.exists():
        print(f"missing fixture: {args.fixture}", file=sys.stderr)
        return 2
    expected = args.fixture.read_bytes()
    if result.stdout != expected:
        actual_path = args.fixture.with_suffix(args.fixture.suffix + ".actual")
        actual_path.write_bytes(result.stdout)
        print(f"oracle mismatch; actual output: {actual_path}", file=sys.stderr)
        print(
            f"expected sha256={hashlib.sha256(expected).hexdigest()}",
            file=sys.stderr,
        )
        print(f"actual   sha256={digest}", file=sys.stderr)
        return 1
    print(f"oracle matched {args.fixture} sha256={digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
