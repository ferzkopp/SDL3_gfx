#!/usr/bin/env python3
"""Generates the seed corpus for the fuzz targets (fuzz/corpus/<target>/seed-*).

The seeds follow the input decoding of each fuzz_<target>.c so that every operation is reached
with plausible parameters; libFuzzer mutates from there. Re-run after changing a decoder:

    python fuzz/make_seeds.py
"""
import os
import random
import struct

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "corpus")
rng = random.Random(1234)


def coord(v):
    # mode 2 = small coordinate: (Sint8) byte + 32
    return bytes([2, (v - 32) & 0xFF])


def write(target, name, data):
    d = os.path.join(ROOT, target)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name), "wb") as f:
        f.write(data)


def primitives():
    for op in range(32):
        cmd = bytes([op])
        cmd += b"".join(coord(rng.randint(-8, 72)) for _ in range(6))
        cmd += struct.pack("<H", rng.randint(0, 40))
        cmd += bytes([rng.randint(0, 255) for _ in range(4)])
        cmd += struct.pack("<I", rng.getrandbits(32))
        n = rng.randint(3, 8)
        cmd += bytes([n]) + b"".join(coord(rng.randint(0, 63)) for _ in range(2 * n))
        if op == 26:
            cmd += b"Fuzz!\0"
        write("primitives", "seed-op%02d" % op, cmd)
    # extreme coordinates (mode 1 = +-32767/-32768)
    write("primitives", "seed-extreme", bytes([23]) + bytes([1, 0, 0x81, 0]) * 3 + b"\xff\x7f" + b"\xff" * 8 + bytes([4]) + bytes([1, 0, 0x81, 0]) * 4)


def rotozoom():
    for i, (w, h, fmt) in enumerate([(8, 8, 0), (16, 9, 1), (13, 17, 2), (5, 31, 6), (32, 32, 2 | 8 | 16), (3, 3, 0 | 8)]):
        bpp = 1 if fmt % 3 == 0 else (3 if fmt % 3 == 1 else 4)
        data = bytes([w - 1, h - 1, fmt]) + bytes(rng.getrandbits(8) for _ in range(w * h * bpp))
        if fmt & 8:
            data += struct.pack("<I", rng.getrandbits(32))
        angle, zx, zy = [rng.choice([0, 30, 90, -45, 360]) * 1024 // 1, 1024, rng.choice([512, 1024, 2048, -1024])]
        for v in (angle, zx, zy):
            data += bytes([3]) + struct.pack("<h", max(-32768, min(32767, v)))
        data += bytes([2, 3, i])
        write("rotozoom", "seed-%d" % i, data)


def imagefilter():
    for op in range(30):
        length = rng.choice([0, 1, 7, 8, 9, 63, 64, 65, 300])
        data = bytes([op | (0x80 if op % 2 else 0)]) + struct.pack("<H", length)
        data += bytes([rng.randint(0, 9), rng.randint(0, 255)])
        data += b"".join(struct.pack("<i", rng.randint(-300, 300)) for _ in range(4))
        data += bytes(rng.getrandbits(8) for _ in range(2 * length))
        write("imagefilter", "seed-op%02d" % op, data)


def font():
    for i, (cw, ch) in enumerate([(8, 8), (5, 7), (9, 18), (1, 1), (32, 32)]):
        glyph = ((cw + 7) // 8) * ch
        data = bytes([cw - 1, ch - 1]) + bytes(rng.getrandbits(8) for _ in range(256 * glyph))
        data += b"Text\x80\xff\0"
        data += b"".join(coord(rng.randint(-4, 60)) + coord(rng.randint(-4, 60)) + bytes([200, 65]) + struct.pack("<I", rng.getrandbits(32)) for _ in range(5))
        write("font", "seed-%d" % i, data)


if __name__ == "__main__":
    primitives()
    rotozoom()
    imagefilter()
    font()
    for t in sorted(os.listdir(ROOT)):
        print(t, len(os.listdir(os.path.join(ROOT, t))), "seeds")
