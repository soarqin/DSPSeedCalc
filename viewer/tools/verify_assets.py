import hashlib
import json
import sys
from pathlib import Path


def main():
    viewer_dir = Path(__file__).resolve().parent.parent
    manifest_path = viewer_dir / "assets" / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    failed = False
    for asset in manifest["assets"]:
        path = viewer_dir / asset["path"]
        if not path.is_file():
            print(f"missing viewer asset: {asset['path']}", file=sys.stderr)
            failed = True
            continue
        payload = path.read_bytes()
        digest = hashlib.sha256(payload).hexdigest()
        if len(payload) != asset["size"] or digest != asset["sha256"]:
            print(
                f"viewer asset mismatch: {asset['path']} "
                f"size={len(payload)} sha256={digest}",
                file=sys.stderr,
            )
            failed = True
    if failed:
        return 1
    print(f"verified {len(manifest['assets'])} viewer assets")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
