#!/usr/bin/env python3
"""Pack GNOME Nibbles .gnl levels into the offline-games binary format."""

from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path


EMPTY = 0
WALL = 1
WARP = 2

DIRECTIONS = {
    "▲": 0,
    "m": 0,
    "▶": 1,
    "p": 1,
    "▼": 2,
    "o": 2,
    "◀": 3,
    "n": 3,
}

WALLS = set("┃━┗┛┏┓┻┣┫┳╋bcdefghijkl")
SOURCES = set("QRSTUVWXYZ")
TARGETS = set("rstuvwxyz")


@dataclass(frozen=True)
class Spawn:
    x: int
    y: int
    direction: int


@dataclass(frozen=True)
class Source:
    ident: int
    x: int
    y: int
    random: bool


@dataclass(frozen=True)
class WarpRecord:
    ident: int
    source_x: int
    source_y: int
    target_x: int
    target_y: int
    flags: int


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source",
        type=Path,
        default=Path("gnome-nibbles-master/data/levels"),
        help="directory containing GNOME Nibbles level*.gnl files",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("assets/nibbles/levels.bytes"),
        help="output levels.bytes path",
    )
    return parser.parse_args()


def warp_id(ch: str) -> int:
    if ch in SOURCES:
        return ord(ch) - ord("A")
    return ord(ch) - ord("a") + ord("A") - ord("A")


def resolve_warps(sources: list[Source], targets: dict[int, tuple[int, int]]) -> list[WarpRecord]:
    by_id: dict[int, list[Source]] = {}
    for source in sources:
        by_id.setdefault(source.ident, []).append(source)

    records: list[WarpRecord] = []
    for source in sources:
        flags = 1 if source.random else 0
        target_x = 0
        target_y = 0
        if not source.random:
            if source.ident in targets:
                target_x, target_y = targets[source.ident]
            else:
                paired = [other for other in by_id[source.ident] if other != source]
                if paired:
                    target_x = paired[0].x
                    target_y = paired[0].y
                    flags |= 2
                else:
                    raise ValueError(
                        f"warp {chr(source.ident + ord('A'))} has no target or pair"
                    )
        records.append(
            WarpRecord(
                ident=source.ident,
                source_x=source.x,
                source_y=source.y,
                target_x=target_x,
                target_y=target_y,
                flags=flags,
            )
        )
    return records


def parse_level(path: Path, source_level: int) -> bytes:
    rows = path.read_text(encoding="utf-8").splitlines()
    if not rows:
        raise ValueError(f"{path}: empty level")
    height = len(rows)
    width = len(rows[0])
    if width > 255 or height > 255:
        raise ValueError(f"{path}: dimensions do not fit in u8")
    if any(len(row) != width for row in rows):
        raise ValueError(f"{path}: ragged rows")

    cells = bytearray(width * height)
    spawns: list[Spawn] = []
    sources: list[Source] = []
    targets: dict[int, tuple[int, int]] = {}

    def set_cell(x: int, y: int, value: int) -> None:
        cells[(y * width) + x] = value

    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch in ".+a":
                set_cell(x, y, EMPTY)
            elif ch in WALLS:
                set_cell(x, y, WALL)
            elif ch in DIRECTIONS:
                set_cell(x, y, EMPTY)
                spawns.append(Spawn(x=x, y=y, direction=DIRECTIONS[ch]))
            elif ch in SOURCES:
                if x == 0 or y == 0:
                    raise ValueError(f"{path}: warp source at edge ({x}, {y})")
                top_left_x = x - 1
                top_left_y = y - 1
                sources.append(
                    Source(
                        ident=warp_id(ch),
                        x=top_left_x,
                        y=top_left_y,
                        random=(ch == "Q"),
                    )
                )
                for dy in range(2):
                    for dx in range(2):
                        set_cell(top_left_x + dx, top_left_y + dy, WARP)
            elif ch in TARGETS:
                set_cell(x, y, EMPTY)
                targets[warp_id(ch)] = (x, y)
            else:
                raise ValueError(f"{path}: unsupported character {ch!r} at ({x}, {y})")

    if not spawns:
        raise ValueError(f"{path}: no worm start positions")
    if len(spawns) > 255:
        raise ValueError(f"{path}: too many start positions")
    warps = resolve_warps(sources, targets)
    if len(warps) > 255:
        raise ValueError(f"{path}: too many warps")

    out = bytearray()
    out += struct.pack("<BBBBB", width, height, source_level, len(spawns), len(warps))
    out += cells
    for spawn in spawns:
        out += struct.pack("<BBB", spawn.x, spawn.y, spawn.direction)
    for warp in warps:
        out += struct.pack(
            "<BBBBBB",
            warp.ident,
            warp.source_x,
            warp.source_y,
            warp.target_x,
            warp.target_y,
            warp.flags,
        )
    return bytes(out)


def main() -> None:
    args = parse_args()
    paths = sorted(args.source.glob("level*.gnl"))
    if not paths:
        raise SystemExit(f"No level*.gnl files found in {args.source}")

    out = bytearray()
    out += b"NIBB"
    out += struct.pack("<HH", 1, len(paths))
    for index, path in enumerate(paths, start=1):
        out += parse_level(path, index)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(out)
    print(f"Wrote {len(paths)} Nibbles levels to {args.output}")


if __name__ == "__main__":
    main()
