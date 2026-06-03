#!/usr/bin/env python3
"""Extract Block Fill (single-rope) levels from the original JindoBlu game.

Run once by a human to (re)generate ``assets/blockfill/levels.bytes``; the game
never depends on this script or on brotli — only the committed asset matters.

The original level bank lives in
``ofg/extracted-unity-full/.../Resources/108_fill/levels_all_ad1_*.bytes``.
Each file is **Brotli-compressed**; the inflated stream is a ``uint16`` level
count followed by that many back-to-back records, read by an **LSB-first** bit
reader (reverse-engineered from libil2cpp ``FillLevelLoader.ReadLevel`` /
``BitReader.ReadBits``). A single-rope record is:

    w (4 bits), h (4 bits), startX (4 bits), startY (4 bits),
    w*h cells x 1 bit row-major  (bit 1 = Missing/hole, 0 = playable),
    solution = (playableCount-1) moves x 2 bits (0:x+1 1:x-1 2:y+1 3:y-1),
    byte-align, f32 difficulty, f32 solutionProb, u32 numVisited.

We decode every level, validate its Hamiltonian solution, sort by difficulty,
split into 4 equal quartiles (Easy=tiniest grids ... VeryHard=largest), cap each
tier to MAX_PER_TIER by uniform stride sampling, and emit a compact uncompressed
blob (see write_asset for the layout) the C++ side reads with no brotli.

Requires the ``brotli`` Python package (``pip install brotli``); needed only to
run this tool, not to build or play the game.
"""

import glob
import os
import re
import struct
import sys

import brotli

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_GLOB = os.path.join(
    REPO_ROOT,
    "ofg/extracted-unity-full/ExportedProject/Assets/Resources/108_fill",
    "levels_all_ad1_*.bytes",
)
OUT_PATH = os.path.join(REPO_ROOT, "assets/blockfill/levels.bytes")

MAGIC = 0x464B4C42  # "BLKF" little-endian
VERSION = 1
TIER_COUNT = 4
MAX_PER_TIER = 300


class BitReader:
    """LSB-first bit reader over a byte buffer (matches the game's BitReader)."""

    def __init__(self, data):
        self.d = data
        self.i = 0
        self.byte = 0
        self.pos = 8  # force a refill on the first bit

    def bit(self):
        if self.pos == 8:
            self.byte = self.d[self.i]
            self.i += 1
            self.pos = 0
        b = (self.byte >> self.pos) & 1
        self.pos += 1
        return b

    def bits(self, n):
        v = 0
        for j in range(n):
            v |= self.bit() << j
        return v

    def align(self):
        self.pos = 8  # discard the rest of the current byte (BitReader.Flush)

    def f32(self):
        v = struct.unpack_from("<f", self.d, self.i)[0]
        self.i += 4
        return v

    def u16(self):
        return self.bits(16)

    def u32(self):
        v = struct.unpack_from("<I", self.d, self.i)[0]
        self.i += 4
        return v


def parse_level(br):
    w = br.bits(4)
    h = br.bits(4)
    sx = br.bits(4)
    sy = br.bits(4)
    if not (1 <= w <= 15 and 1 <= h <= 15):
        raise ValueError(f"bad dims {w}x{h}")
    grid = [[br.bit() for _ in range(w)] for _ in range(h)]  # 1 = missing/hole
    missing = sum(sum(row) for row in grid)
    playable = w * h - missing
    if playable < 2 or not (0 <= sx < w and 0 <= sy < h) or grid[sy][sx] == 1:
        raise ValueError("bad start / playable count")
    sol = [(sx, sy)]
    x, y = sx, sy
    for _ in range(playable - 1):
        d = br.bits(2)
        if d == 0:
            x += 1
        elif d == 1:
            x -= 1
        elif d == 2:
            y += 1
        else:
            y -= 1
        sol.append((x, y))
    br.align()
    diff = br.f32()
    _prob = br.f32()
    _nvisited = br.u32()
    return {"w": w, "h": h, "start": (sx, sy), "playable": playable,
            "diff": diff, "sol": sol, "grid": grid}


def is_valid(lv):
    """True if the stored solution is a legal Hamiltonian path over the board."""
    w, h = lv["w"], lv["h"]
    seen = set()
    prev = None
    for (x, y) in lv["sol"]:
        if not (0 <= x < w and 0 <= y < h):
            return False
        if lv["grid"][y][x] == 1:
            return False
        if (x, y) in seen:
            return False
        if prev is not None and abs(prev[0] - x) + abs(prev[1] - y) != 1:
            return False
        seen.add((x, y))
        prev = (x, y)
    return len(seen) == lv["playable"] and 0.0 <= lv["diff"] < 1e7


def decode_file(path):
    raw = open(path, "rb").read()
    out = brotli.decompress(raw)
    br = BitReader(out)
    count = br.u16()
    levels = []
    for _ in range(count):
        lv = parse_level(br)
        if is_valid(lv):
            levels.append(lv)
    return levels


def encode_record(lv):
    w, h = lv["w"], lv["h"]
    sx, sy = lv["start"]
    rec = bytearray([w, h, sx, sy])
    mask = bytearray((w * h + 7) // 8)
    for idx in range(w * h):
        if lv["grid"][idx // w][idx % w] == 1:  # bit set = missing/hole
            mask[idx // 8] |= 1 << (idx % 8)
    rec += mask
    return bytes(rec)


def stride_sample(items, cap):
    if len(items) <= cap:
        return items
    step = len(items) / cap
    return [items[int(k * step)] for k in range(cap)]


def write_asset(tiers):
    """Layout (little-endian):
      u32 magic, u16 version, u16 tierCount, u16 count[0..3]   (16-byte header)
      then every level in tier order, ascending difficulty within a tier:
        u8 w, u8 h, u8 startX, u8 startY, ceil(w*h/8) bytes missing-bitmask
        (row-major, LSB-first; bit set = MISSING/hole; index = y*w + x)
    """
    counts = [len(t) for t in tiers]
    body = bytearray()
    body += struct.pack("<IHH", MAGIC, VERSION, TIER_COUNT)
    body += struct.pack("<HHHH", *counts)
    for tier in tiers:
        for lv in tier:
            body += encode_record(lv)
    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    with open(OUT_PATH, "wb") as f:
        f.write(body)
    return counts, len(body)


def main():
    files = sorted(glob.glob(SRC_GLOB),
                   key=lambda p: int(re.search(r"_(\d+)\.bytes$", p).group(1)))
    if not files:
        print(f"ERROR: no source files matched {SRC_GLOB}", file=sys.stderr)
        return 1

    levels = []
    for path in files:
        levels.extend(decode_file(path))
    if not levels:
        print("ERROR: decoded 0 levels", file=sys.stderr)
        return 1

    # Global ascending-difficulty order, then split into 4 equal quartiles.
    levels.sort(key=lambda lv: lv["diff"])
    n = len(levels)
    tiers = []
    for t in range(TIER_COUNT):
        lo = (t * n) // TIER_COUNT
        hi = ((t + 1) * n) // TIER_COUNT
        tiers.append(stride_sample(levels[lo:hi], MAX_PER_TIER))

    counts, size = write_asset(tiers)
    names = ["Easy", "Medium", "Hard", "VeryHard"]
    print(f"Decoded {n} valid levels from {len(files)} files.")
    for name, tier in zip(names, tiers):
        ws = [lv["w"] for lv in tier]
        hs = [lv["h"] for lv in tier]
        ds = [lv["diff"] for lv in tier]
        print(f"  {name:9s}: {len(tier):4d} levels  "
              f"grid {min(ws)}x{min(hs)}..{max(ws)}x{max(hs)}  "
              f"diff {min(ds):.0f}..{max(ds):.0f}")
    print(f"Wrote {OUT_PATH} ({size} bytes, counts={counts}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
