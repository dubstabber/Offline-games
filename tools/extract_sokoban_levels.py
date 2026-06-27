#!/usr/bin/env python3
"""Pack Sokobang/KSokoban JSON levels into the offline-games binary format."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


TIERS = (
    (3, 6, 8),       # Easy: Microban, Mas Microban, LOMA.
    (0, 1, 2),       # Medium: Sasquatch I-III.
    (4, 5, 7, 9),    # Hard: Sasquatch IV-VII.
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source",
        type=Path,
        default=Path("sokobang-master/public/levels"),
        help="directory containing levels.json and level_<set>_<level>.json",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("assets/sokoban/levels.bytes"),
        help="output levels.bytes path",
    )
    return parser.parse_args()


def read_level(root: Path, set_index: int, level_index: int) -> tuple[int, int, bytes]:
    path = root / f"level_{set_index}_{level_index}.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    width = int(data["width"])
    height = int(data["height"])
    rows = data["data"]
    if len(rows) != height:
        raise ValueError(f"{path}: height {height} but has {len(rows)} rows")
    cells = bytearray()
    for row in rows:
        if len(row) != width:
            raise ValueError(f"{path}: width {width} but row has {len(row)} chars")
        for cell in row:
            if cell not in " #.$*@+":
                raise ValueError(f"{path}: unsupported cell {cell!r}")
            cells.append(ord(cell))
    if width <= 0 or width > 255 or height <= 0 or height > 255:
        raise ValueError(f"{path}: dimensions out of u8 range")
    return width, height, bytes(cells)


def main() -> None:
    args = parse_args()
    levels_meta = json.loads((args.source / "levels.json").read_text(encoding="utf-8"))
    tiers: list[list[tuple[int, int, int, int, bytes]]] = []
    for tier_sets in TIERS:
        packed_levels: list[tuple[int, int, int, int, bytes]] = []
        for set_index in tier_sets:
            count = int(levels_meta[set_index]["levels"])
            for level_index in range(count):
                width, height, cells = read_level(args.source, set_index, level_index)
                packed_levels.append((width, height, set_index, level_index + 1, cells))
        tiers.append(packed_levels)

    out = bytearray()
    out += b"SOKO"
    out += struct.pack("<HH", 1, len(tiers))
    for tier in tiers:
        if len(tier) > 65535:
            raise ValueError("tier has too many levels for u16")
        out += struct.pack("<H", len(tier))
    for tier in tiers:
        for width, height, set_index, source_level, cells in tier:
            out += struct.pack("<BBBH", width, height, set_index, source_level)
            out += cells

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(out)
    total = sum(len(tier) for tier in tiers)
    counts = "/".join(str(len(tier)) for tier in tiers)
    print(f"Wrote {total} Sokoban levels ({counts}) to {args.output}")


if __name__ == "__main__":
    main()
