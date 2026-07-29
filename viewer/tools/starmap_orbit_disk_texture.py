"""Rebuild and verify the starmap orbit disk texture.

Source in Dyson Sphere Program 0.10.34.28529 (`sharedassets0.assets`):

    path ID 1083   orbit-disk   512 x 512, BC7, 10 mips, colour space 1 (sRGB)

`UIStarmapPlanet` parents this quad under the orbit LineRenderer, so it fills the orbital plane
just inside the circle. The decoded image is greyscale with R = G = B everywhere and alpha fixed
at 1, so the viewer only needs one channel. The output file is the tightly packed 8-bit red mip
chain, largest level first, stored byte for byte as the source decodes: `starmap_disk.fs` applies
the sRGB to linear transfer that Unity's colour import would, which keeps the file lossless
against the game asset instead of baking a lower precision linear ramp.

Run without arguments to verify the committed file. Pass the game's `DSPGAME_Data` directory to
regenerate it; that path needs `UnityPy` and `texture2ddecoder`.
"""

import hashlib
import math
import sys
from pathlib import Path


PATH_ID = 1083
WIDTH = 512
HEIGHT = 512
MIPMAPS = 10
EXPECTED_SIZE = 349525
EXPECTED_SHA256 = "73366399a14a853e0e342f900fdec61bc8ccbf6e625e22ad984e117687916a62"


def output_path():
    return Path(__file__).resolve().parent.parent / "assets" / "textures" / "starmap_orbit_disk.bin"


def extract(data_dir):
    import os

    import UnityPy
    import texture2ddecoder

    env = UnityPy.load(str(Path(data_dir) / "sharedassets0.assets"))
    obj = next(o for o in env.objects if o.path_id == PATH_ID)
    texture = obj.read()
    stream = texture.m_StreamData
    if stream is not None and stream.path:
        source = Path(data_dir) / os.path.basename(stream.path)
        with source.open("rb") as handle:
            handle.seek(stream.offset)
            payload = handle.read(stream.size)
    else:
        payload = bytes(texture.image_data)
    width, height = WIDTH, HEIGHT
    result = bytearray()
    offset = 0
    for _ in range(MIPMAPS):
        block = math.ceil(width / 4) * math.ceil(height / 4) * 16
        decoded = texture2ddecoder.decode_bc7(payload[offset:offset + block], width, height)
        # decode_bc7 returns BGRA, so the red channel starts at byte 2.
        red = decoded[2::4]
        if bytes(decoded[0::4]) != bytes(red) or bytes(decoded[1::4]) != bytes(red):
            raise ValueError("orbit-disk is no longer greyscale; the viewer stores one channel")
        result.extend(red)
        offset += block
        width = max(1, width // 2)
        height = max(1, height // 2)
    if offset != len(payload):
        raise ValueError("unexpected mip payload size for path ID %d" % PATH_ID)
    return bytes(result)


def main():
    if len(sys.argv) > 2:
        raise SystemExit("usage: %s [DSPGAME_Data]" % Path(sys.argv[0]).name)
    path = output_path()
    if len(sys.argv) == 2:
        data = extract(sys.argv[1])
        path.write_bytes(data)
    else:
        data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if len(data) != EXPECTED_SIZE:
        raise ValueError("expected %d bytes, got %d" % (EXPECTED_SIZE, len(data)))
    if EXPECTED_SHA256 and digest != EXPECTED_SHA256:
        raise ValueError("sha256 mismatch %s" % digest)
    print("starmap_orbit_disk.bin: %d bytes %s" % (len(data), digest))


if __name__ == "__main__":
    main()
