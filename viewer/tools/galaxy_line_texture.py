import hashlib
import math
import struct
import sys
from pathlib import Path


EXPECTED_SOURCE_SHA256 = "d31145fb70867cb6fad69e0a10bde37a9f10def6d6302de39036848fa09a397a"
EXPECTED_OUTPUT_SHA256 = "10fda98434a6d8c1165d0c914bb9c095aaa5eace66d39d78f3a350c4c090ad37"
EXPECTED_OUTPUT_SIZE = 699056


def expand_565(value):
    return value >> 11, (value >> 5) & 63, value & 31


def expand_5(value):
    return (value << 3) | (value >> 2)


def expand_6(value):
    return (value << 2) | (value >> 4)


def srgb_to_linear(value):
    value /= 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def decode_bc1(data, width, height):
    pixels = [(0, 0, 0)] * (width * height)
    offset = 0
    for block_y in range(math.ceil(height / 4)):
        for block_x in range(math.ceil(width / 4)):
            color0, color1, indices = struct.unpack_from("<HHI", data, offset)
            offset += 8
            r0, g0, b0 = expand_565(color0)
            r1, g1, b1 = expand_565(color1)
            c0 = (expand_5(r0), expand_6(g0), expand_5(b0))
            c1 = (expand_5(r1), expand_6(g1), expand_5(b1))
            colors = [c0, c1]
            if color0 > color1:
                colors.append(tuple((2 * a + b) // 3 for a, b in zip(c0, c1)))
                colors.append(tuple((a + 2 * b) // 3 for a, b in zip(c0, c1)))
            else:
                colors.append(tuple((a + b) // 2 for a, b in zip(c0, c1)))
                colors.append((0, 0, 0))
            for y in range(4):
                for x in range(4):
                    px = block_x * 4 + x
                    py = block_y * 4 + y
                    if px < width and py < height:
                        index = (indices >> (2 * (y * 4 + x))) & 3
                        pixels[py * width + px] = colors[index]
    return pixels


def convert(source):
    payload = source.read_bytes()
    if hashlib.sha256(payload).hexdigest() != EXPECTED_SOURCE_SHA256:
        raise ValueError("source is not the extracted sharedassets0 texture path ID 531")
    result = bytearray()
    offset = 0
    width, height = 256, 128
    for _ in range(9):
        size = max(8, math.ceil(width / 4) * math.ceil(height / 4) * 8)
        for r, g, b in decode_bc1(payload[offset:offset + size], width, height):
            result.extend(struct.pack("<4f", srgb_to_linear(r), srgb_to_linear(g), srgb_to_linear(b), 1.0))
        offset += size
        width = max(1, width // 2)
        height = max(1, height // 2)
    if offset != len(payload):
        raise ValueError("unexpected BC1 mip payload size")
    return bytes(result)


def main():
    output = Path(__file__).resolve().parent.parent / "assets" / "textures" / "galaxy_line_texture.bin"
    if len(sys.argv) == 1:
        data = output.read_bytes()
    elif len(sys.argv) == 2:
        data = convert(Path(sys.argv[1]))
        output.write_bytes(data)
    else:
        raise SystemExit(f"usage: {Path(sys.argv[0]).name} [sharedassets0.531.texture.bin]")
    digest = hashlib.sha256(data).hexdigest()
    if len(data) != EXPECTED_OUTPUT_SIZE or digest != EXPECTED_OUTPUT_SHA256:
        raise ValueError("galaxy_line_texture.bin does not match the extracted nine-mip texture")
    print(f"verified {output.name}: {digest}")


if __name__ == "__main__":
    main()
