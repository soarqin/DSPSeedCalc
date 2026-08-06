#!/usr/bin/env python3
"""Compare the in-game vein reference against the C++ generator.

The reference files are produced by tools/vein-oracle-mod running inside the
shipped game, so they carry the game's own results for the same seeds, including
the Unity math routines that live in engine native code.

Two checks run:

1. The prototype fingerprint the mod captured from the live LDB is compared
   field by field against the repository's Prototypes snapshot. A mismatch means
   the running game was not using the data this repository generates from, so
   the vein comparison would be meaningless.
2. The per-planet node counts and base amounts are compared exactly.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys


def load_json(path: pathlib.Path):
    rows = json.loads(path.read_text(encoding="utf-8-sig"))
    if isinstance(rows, dict) and "dataArray" in rows:
        rows = rows["dataArray"]
    return rows


def fmt_float(value: float) -> str:
    """Match .NET's "R" round-trip formatting for the values in these tables."""
    if value == int(value) and abs(value) < 1e16:
        return str(int(value))
    text = repr(float(value))
    return text


def fmt_int_list(values) -> str:
    if values is None:
        return "-"
    if len(values) == 0:
        return "-"
    return ";".join(str(int(v)) for v in values)


def fmt_float_list(values) -> str:
    if values is None:
        return "-"
    if len(values) == 0:
        return "-"
    return ";".join(fmt_float(v) for v in values)


def parse_kv_line(line: str) -> dict:
    fields = {}
    for token in line.split(" "):
        if "=" not in token:
            continue
        key, _, value = token.partition("=")
        fields[key] = value
    return fields


def check_protos(game_path: pathlib.Path, prototypes: pathlib.Path) -> list[str]:
    problems: list[str] = []
    themes = {int(r["ID"]): r for r in load_json(prototypes / "ThemeProtoSet.json")}
    veins = {int(r["ID"]): r for r in load_json(prototypes / "VeinProtoSet.json")}

    seen_themes = 0
    seen_veins = 0
    for raw in game_path.read_text(encoding="utf-8").splitlines():
        if raw.startswith("theme "):
            seen_themes += 1
            got = parse_kv_line(raw)
            theme_id = int(got["id"])
            want = themes.get(theme_id)
            if want is None:
                problems.append(f"theme {theme_id} missing from snapshot")
                continue
            expected = {
                "distribute": str(int(want.get("Distribute", 0))),
                "planetType": str(int(want.get("PlanetType", 0))),
                "waterItemId": str(int(want.get("WaterItemId", 0))),
                "waterHeight": fmt_float(want.get("WaterHeight", 0.0)),
                "modX0": fmt_float(want.get("ModX", {}).get("x", 0.0)),
                "modX1": fmt_float(want.get("ModX", {}).get("y", 0.0)),
                "modY0": fmt_float(want.get("ModY", {}).get("x", 0.0)),
                "modY1": fmt_float(want.get("ModY", {}).get("y", 0.0)),
                "algos": fmt_int_list(want.get("Algos")),
                "veinSpot": fmt_int_list(want.get("VeinSpot")),
                "veinCount": fmt_float_list(want.get("VeinCount")),
                "veinOpacity": fmt_float_list(want.get("VeinOpacity")),
                "rareVeins": fmt_int_list(want.get("RareVeins")),
                "rareSettings": fmt_float_list(want.get("RareSettings")),
            }
            for key, value in expected.items():
                if got.get(key) != value:
                    problems.append(
                        f"theme {theme_id} {key}: game={got.get(key)!r} snapshot={value!r}")
        elif raw.startswith("vein "):
            seen_veins += 1
            got = parse_kv_line(raw)
            vein_id = int(got["id"])
            want = veins.get(vein_id)
            if want is None:
                problems.append(f"vein {vein_id} missing from snapshot")
                continue
            expected = {
                "modelIndex": str(int(want.get("ModelIndex", 0))),
                "modelCount": str(int(want.get("ModelCount", 0))),
                "miningItem": str(int(want.get("MiningItem", 0))),
            }
            for key, value in expected.items():
                if got.get(key) != value:
                    problems.append(
                        f"vein {vein_id} {key}: game={got.get(key)!r} snapshot={value!r}")

    if seen_themes != len(themes):
        problems.append(f"theme count: game={seen_themes} snapshot={len(themes)}")
    if seen_veins != len(veins):
        problems.append(f"vein count: game={seen_veins} snapshot={len(veins)}")
    return problems


def parse_dump(text: str) -> dict:
    galaxies: dict = {}
    current = None
    for raw in text.splitlines():
        if raw.startswith("#"):
            continue
        if raw.startswith("galaxy "):
            fields = parse_kv_line(raw)
            current = {
                "stars": fields["stars"],
                "birthPlanet": fields["birthPlanet"],
                "planets": {},
            }
            galaxies[fields["seed"]] = current
        elif raw.startswith("planet "):
            if current is None:
                raise ValueError("planet line before galaxy line")
            fields = parse_kv_line(raw)
            current["planets"][fields["id"]] = fields
    return galaxies


def check_dump(game_text: str, cpp_text: str) -> list[str]:
    problems: list[str] = []
    game = parse_dump(game_text)
    cpp = parse_dump(cpp_text)

    missing = set(game) - set(cpp)
    if missing:
        problems.append(
            f"{len(missing)} seed(s) present in the reference but not in the C++ dump: "
            f"{sorted(missing, key=int)[:8]}")
        return problems

    for seed in sorted(game, key=int):
        g, c = game[seed], cpp[seed]
        for key in ("stars", "birthPlanet"):
            if g[key] != c[key]:
                problems.append(f"seed {seed} {key}: game={g[key]} cpp={c[key]}")
        if set(g["planets"]) != set(c["planets"]):
            only_game = sorted(set(g["planets"]) - set(c["planets"]), key=int)
            only_cpp = sorted(set(c["planets"]) - set(g["planets"]), key=int)
            problems.append(
                f"seed {seed} planet ids differ: only-game={only_game[:8]} only-cpp={only_cpp[:8]}")
            continue
        for planet_id in sorted(g["planets"], key=int):
            gp, cp = g["planets"][planet_id], c["planets"][planet_id]
            for key in ("algo", "theme", "nodes", "amounts", "height", "biomo"):
                if gp.get(key) != cp.get(key):
                    problems.append(
                        f"seed {seed} planet {planet_id} {key}:\n"
                        f"    game={gp.get(key)}\n"
                        f"    cpp ={cp.get(key)}")
    return problems


def coverage(game_text: str, cpp_text: str) -> str:
    game, cpp = parse_dump(game_text), parse_dump(cpp_text)
    gas = nongas = 0
    for seed, g in game.items():
        if seed not in cpp:
            continue
        for gp in g["planets"].values():
            if gp.get("algo") == "0":
                gas += 1
            else:
                nongas += 1
    return (f"comparing heightData and biomoData on every planet: "
            f"{nongas} generated + {gas} gas, no exclusions")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=pathlib.Path,
                        help="dspugen_vein_amount_oracle built with the parity dump mode")
    parser.add_argument("--game", type=pathlib.Path,
                        default=pathlib.Path("tests/fixtures/game/vein-parity-game.txt"))
    parser.add_argument("--game-protos", type=pathlib.Path,
                        default=pathlib.Path("tests/fixtures/game/vein-parity-game-protos.txt"))
    parser.add_argument("--prototypes", type=pathlib.Path,
                        default=pathlib.Path("Prototypes"))
    parser.add_argument("--verts", type=pathlib.Path, default=None,
                        help="precision-200 vertex grid captured from the game; "
                             "required for bit-exact terrain, since Vector3.Slerp "
                             "lives in engine native code")
    args = parser.parse_args()

    if not args.game.exists():
        print(f"missing game reference: {args.game}", file=sys.stderr)
        return 2

    proto_problems = check_protos(args.game_protos, args.prototypes)
    if proto_problems:
        print("prototype fingerprint mismatch; the running game did not use this "
              "repository's Prototypes snapshot:", file=sys.stderr)
        for problem in proto_problems[:40]:
            print(f"  {problem}", file=sys.stderr)
        return 1
    print("prototype fingerprint matches Prototypes/")

    # Drive the C++ side over exactly the seeds the reference run captured, so
    # the harness does not need to be told the range twice.
    game_text = args.game.read_text(encoding="utf-8")
    seeds = sorted(int(s) for s in parse_dump(game_text))
    if not seeds:
        print(f"no galaxies in {args.game}", file=sys.stderr)
        return 2

    command = [str(args.executable.resolve())]
    if args.verts is not None:
        command += ["--verts", str(args.verts.resolve())]
    command += ["--parity", str(seeds[0]), str(seeds[-1])]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            cwd=pathlib.Path.cwd(), check=False)
    if result.returncode:
        sys.stderr.buffer.write(result.stderr)
        return result.returncode

    cpp_text = result.stdout.decode("utf-8")
    print(coverage(game_text, cpp_text))
    problems = check_dump(game_text, cpp_text)
    if problems:
        actual = args.game.with_name("vein-parity-cpp.txt")
        actual.write_text(cpp_text, encoding="utf-8")
        print(f"{len(problems)} mismatch(es); C++ dump written to {actual}",
              file=sys.stderr)
        for problem in problems[:40]:
            print(f"  {problem}", file=sys.stderr)
        return 1

    reference = parse_dump(game_text)
    planets = sum(len(g["planets"]) for g in reference.values())
    print(f"vein parity matched the in-game reference: "
          f"{planets} planets across {len(reference)} galaxies "
          f"(seeds {seeds[0]}-{seeds[-1]})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
