#!/usr/bin/env python3
"""Audit parity-sensitive compile commands for forbidden FP options."""

from __future__ import annotations

import argparse
import json
import pathlib
import shlex
import sys


SOURCES = {
    "galaxy.cc", "star.cc", "planet.cc", "protoset.cc", "namegen.cc",
    "dotnet35random.cc",
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("compile_commands", type=pathlib.Path)
    args = parser.parse_args()
    commands = json.loads(args.compile_commands.read_text(encoding="utf-8"))
    errors: list[str] = []
    seen: set[str] = set()
    for entry in commands:
        source = pathlib.Path(entry["file"]).name
        if source not in SOURCES:
            continue
        seen.add(source)
        tokens = entry.get("arguments") or shlex.split(entry["command"], posix=False)
        joined = " ".join(tokens)
        if "-ffast-math" in joined or "-Ofast" in joined or "/fp:fast" in joined:
            errors.append(f"{source}: forbidden floating-point option")
        if any("clang" in pathlib.Path(token).name.lower() or "g++" in pathlib.Path(token).name.lower()
               for token in tokens[:1]):
            if "-fno-fast-math" not in tokens:
                errors.append(f"{source}: missing -fno-fast-math")
            if "-ffp-contract=off" not in tokens:
                errors.append(f"{source}: missing -ffp-contract=off")
    missing = SOURCES - seen
    errors.extend(f"missing compile command: {source}" for source in sorted(missing))
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"audited {len(seen)} parity-sensitive translation units")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
