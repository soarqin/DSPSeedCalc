"""Rebuild and verify the Galaxy Select alpha-only textures.

Sources in Dyson Sphere Program 0.10.34.28529 (`sharedassets0.assets`):

    path ID 602   grid-256-dot            256 x 256, BC7, 9 mips
    path ID 666   grid-256                256 x 256, BC7, 9 mips
    path ID 823   galaxy-select-frame     1024 x 1024, RGBA32, 11 mips
    path ID 903   circle-thin             256 x 256, BC7, 9 mips

All three store RGB = 255 everywhere and carry their pattern purely in alpha, so
the viewer only needs the alpha channel. Each output file is the tightly packed
8-bit alpha mip chain, largest level first.

Run without arguments to verify the committed files. Pass the game's
`DSPGAME_Data` directory to regenerate them; that path needs `UnityPy` and
`texture2ddecoder`.
"""

import hashlib
import math
import sys
from pathlib import Path


TEXTURES = {
    "galaxy_grid_dot.bin": {
        "path_id": 602,
        "codec": "bc7",
        "width": 256,
        "height": 256,
        "mipmaps": 9,
        "size": 87381,
        "sha256": "3dddc88d77b6d55b0f2bd99964d7d276118b346424e18830e3cc3193ab998a04",
    },
    "galaxy_grid_line.bin": {
        "path_id": 666,
        "codec": "bc7",
        "width": 256,
        "height": 256,
        "mipmaps": 9,
        "size": 87381,
        "sha256": "4b4301b0cecc99764d37dbfc0dbdecea719245182fca5336d36f5a8c0c0af72f",
    },
    "galaxy_grid_frame.bin": {
        "path_id": 823,
        "codec": "rgba32",
        "width": 1024,
        "height": 1024,
        "mipmaps": 11,
        "size": 1398101,
        "sha256": "2390e7949c01fca0a675615a72d522c67575bf03fc643ee32267145c0cb237a3",
    },
    "galaxy_star_ring.bin": {
        "path_id": 903,
        "codec": "bc7",
        "width": 256,
        "height": 256,
        "mipmaps": 9,
        "size": 87381,
        "sha256": "002d2eedec4d4888f958f0da359852b7295da6d19fb491469e46f7e37f325a9d",
    },
}


def output_dir():
    return Path(__file__).resolve().parent.parent / "assets" / "textures"


def extract(data_dir, spec):
    import os

    import UnityPy
    import texture2ddecoder

    env = UnityPy.load(str(Path(data_dir) / "sharedassets0.assets"))
    obj = next(o for o in env.objects if o.path_id == spec["path_id"])
    texture = obj.read()
    stream = texture.m_StreamData
    if stream is not None and stream.path:
        source = Path(data_dir) / os.path.basename(stream.path)
        with source.open("rb") as handle:
            handle.seek(stream.offset)
            payload = handle.read(stream.size)
    else:
        payload = bytes(texture.image_data)
    width, height = spec["width"], spec["height"]
    result = bytearray()
    offset = 0
    for _ in range(spec["mipmaps"]):
        if spec["codec"] == "bc7":
            block = math.ceil(width / 4) * math.ceil(height / 4) * 16
            decoded = texture2ddecoder.decode_bc7(payload[offset:offset + block], width, height)
        else:
            block = width * height * 4
            decoded = payload[offset:offset + block]
        result.extend(decoded[3::4])
        offset += block
        width = max(1, width // 2)
        height = max(1, height // 2)
    if offset != len(payload):
        raise ValueError(f"unexpected mip payload size for path ID {spec['path_id']}")
    return bytes(result)


def main():
    if len(sys.argv) > 2:
        raise SystemExit(f"usage: {Path(sys.argv[0]).name} [DSPGAME_Data]")
    target = output_dir()
    for name, spec in TEXTURES.items():
        path = target / name
        if len(sys.argv) == 2:
            data = extract(sys.argv[1], spec)
            path.write_bytes(data)
        else:
            data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if len(data) != spec["size"]:
            raise ValueError(f"{name}: expected {spec['size']} bytes, got {len(data)}")
        if spec["sha256"] and digest != spec["sha256"]:
            raise ValueError(f"{name}: sha256 mismatch {digest}")
        print(f"{name}: {len(data)} bytes {digest}")


if __name__ == "__main__":
    main()
