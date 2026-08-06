#!/usr/bin/env python3
"""Audit parity-sensitive compile commands for forbidden FP options.

Coverage is not limited to dspugen's own translation units: util/maths.hh and
vectors.hh are inline headers, so the CLI host, the parity oracle and every plugin
compile parity-sensitive arithmetic into their own TUs and need the same strict
floating-point contract.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import shlex
import sys


# Must be present in every build; a missing entry is an error.
REQUIRED_SUFFIXES = (
    "dspugen/galaxy.cc",
    "dspugen/star.cc",
    "dspugen/planet.cc",
    "dspugen/protoset.cc",
    "dspugen/namegen.cc",
    "dspugen/vein/vein_amounts.cc",
    "dspugen/terrain/simplex_noise.cc",
    "dspugen/terrain/unity_math.cc",
    "dspugen/terrain/ucrt_trig.cc",
    "dspugen/terrain/planet_raw_data.cc",
    "dspugen/terrain/planet_algorithm.cc",
    "dspugen/terrain/planet_geometry.cc",
    "dspugen/util/dotnet35random.cc",
    "cli/main.cc",
    "cli/filter.cc",
)

# Audited when present, but configuration dependent: the oracle needs
# DSP_BUILD_ORACLE, the benchmark plugins need DSP_BUILD_BENCHMARK_PLUGINS, and a
# plugin build directory may hold any subset of filters/.
OPTIONAL_FRAGMENTS = (
    "tests/oracle.cc",
    "tests/vein_amount_oracle.cc",
    "/filters/",
    "benchmarks/plugins/",
)

FORBIDDEN = ("-ffast-math", "-Ofast", "/fp:fast", "-funsafe-math-optimizations",
             "-fassociative-math", "-freciprocal-math")
GNU_REQUIRED = ("-fno-fast-math", "-ffp-contract=off")


def normalized(path: str) -> str:
    return path.replace("\\", "/")


GNU_MARKERS = ("clang", "gcc", "g++", "c++", "cc", "cpp")


def compiler_family(tokens: list[str]) -> str:
    """Classify the driver so the right flag set is required.

    CMake records whatever driver it found, which on many toolchains is a plain `c++`
    or `cc` symlink rather than `g++`/`gcc`; those have to be recognized as GNU-family
    or the audit would reject a correctly configured build.
    """
    if not tokens:
        return "unknown"
    name = pathlib.Path(tokens[0]).name.lower()
    stem = name[:-4] if name.endswith(".exe") else name
    if "clang-cl" in stem or stem == "cl":
        return "msvc"
    if any(marker in stem for marker in GNU_MARKERS):
        return "gnu"
    return "unknown"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("compile_commands", type=pathlib.Path)
    args = parser.parse_args()
    commands = json.loads(args.compile_commands.read_text(encoding="utf-8"))
    errors: list[str] = []
    seen: set[str] = set()
    audited = 0
    for entry in commands:
        source = normalized(entry["file"])
        required = next((suffix for suffix in REQUIRED_SUFFIXES
                         if source.endswith(suffix)), None)
        if required is None and not any(fragment in source
                                        for fragment in OPTIONAL_FRAGMENTS):
            continue
        if required is not None:
            seen.add(required)
        audited += 1
        label = "/".join(source.rsplit("/", 2)[-2:])
        tokens = entry.get("arguments") or shlex.split(entry["command"], posix=False)
        joined = " ".join(tokens)
        for option in FORBIDDEN:
            if option in joined:
                errors.append(f"{label}: forbidden floating-point option {option}")
        family = compiler_family(tokens)
        if family == "gnu":
            for option in GNU_REQUIRED:
                if option not in tokens:
                    errors.append(f"{label}: missing {option}")
        elif family == "msvc":
            if "/fp:strict" not in tokens:
                errors.append(f"{label}: missing /fp:strict")
        else:
            errors.append(f"{label}: unrecognized compiler {tokens[:1]}")
    errors.extend(f"missing compile command: {suffix}"
                  for suffix in sorted(set(REQUIRED_SUFFIXES) - seen))
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"audited {audited} parity-sensitive translation units")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
