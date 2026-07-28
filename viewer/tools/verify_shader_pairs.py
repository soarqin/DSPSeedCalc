import re
import sys
from pathlib import Path


PRECISION = re.compile(r"\b(?:lowp|mediump|highp)\s+")


def normalized(path):
    lines = path.read_text(encoding="utf-8").splitlines()
    result = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("#version") or stripped.startswith("precision "):
            continue
        result.append(PRECISION.sub("", line).strip())
    while result and not result[-1]:
        result.pop()
    return "\n".join(result)


def main():
    viewer_dir = Path(__file__).resolve().parent.parent
    desktop_dir = viewer_dir / "shaders" / "desktop"
    web_dir = viewer_dir / "shaders" / "web"
    desktop = {path.name: path for path in desktop_dir.iterdir() if path.suffix in {".vs", ".fs"}}
    web = {path.name: path for path in web_dir.iterdir() if path.suffix in {".vs", ".fs"}}
    failed = False
    for name in sorted(set(desktop) | set(web)):
        if name not in desktop or name not in web:
            print(f"unpaired viewer shader: {name}", file=sys.stderr)
            failed = True
            continue
        if normalized(desktop[name]) != normalized(web[name]):
            print(f"viewer shader bodies differ: {name}", file=sys.stderr)
            failed = True
    if failed:
        return 1
    print(f"verified {len(desktop)} desktop/web shader pairs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
