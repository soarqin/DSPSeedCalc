"""Rebuild and verify the Galaxy Select background star noise texture.

Source in Dyson Sphere Program 0.10.34.28529 (`sharedassets0.assets`):

    path ID 605   random-colors-128-2   128 x 128, RGBA32, 1 mip, linear, point filter

The material `bgstars-galaxy-select` feeds it to `Universe/Star/Star Mono-Layer`
as `_NoiseMap`. The texture is a linear colour texture, so the output file is the
raw RGBA8 payload with no conversion.

Run without arguments to verify the committed file. Pass the game's `DSPGAME_Data`
directory to regenerate it; that path needs `UnityPy`.
"""

import hashlib
import sys
from pathlib import Path


PATH_ID = 605
WIDTH = 128
HEIGHT = 128
SIZE = 65536
SHA256 = "39b586fa3699a83d35745eededc3832902087375288089870afa22106503a7ad"
OUTPUT = "galaxy_bgstars_noise.bin"


def extract(data_dir):
    import os

    import UnityPy

    env = UnityPy.load(str(Path(data_dir) / "sharedassets0.assets"))
    obj = next(o for o in env.objects if o.path_id == PATH_ID)
    texture = obj.read()
    stream = texture.m_StreamData
    if stream is not None and stream.path:
        source = Path(data_dir) / os.path.basename(stream.path)
        with source.open("rb") as handle:
            handle.seek(stream.offset)
            return handle.read(stream.size)
    return bytes(texture.image_data)


def main():
    if len(sys.argv) > 2:
        raise SystemExit(f"usage: {Path(sys.argv[0]).name} [DSPGAME_Data]")
    path = Path(__file__).resolve().parent.parent / "assets" / "textures" / OUTPUT
    if len(sys.argv) == 2:
        data = extract(sys.argv[1])
        path.write_bytes(data)
    else:
        data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if len(data) != SIZE:
        raise ValueError(f"{OUTPUT}: expected {SIZE} bytes, got {len(data)}")
    if SHA256 and digest != SHA256:
        raise ValueError(f"{OUTPUT}: sha256 mismatch {digest}")
    print(f"{OUTPUT}: {len(data)} bytes {digest}")


if __name__ == "__main__":
    main()
