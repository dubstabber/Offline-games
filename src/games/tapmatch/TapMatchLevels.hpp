#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace og {

// One tile of an authored Tap Match board, already converted to the port's fine
// grid: (x, y) is the top-left of a kTileSpan x kTileSpan footprint and a higher
// layer covers the tiles it overlaps. Decoded from the original game's level data.
struct PlacedTile {
    int layer = 0;
    int x = 0;
    int y = 0;
};

// One original Tap Match level: the fixed board geometry plus the two knobs the
// original feeds its solver. The original stores *no* icons — iconsCount says how
// many distinct icons to use and initStackSize bounds the working holder — and a
// solvable colouring is generated at play time. TapMatchBoard reproduces that, so
// these two values map onto its iconVariety / holderBudget.
struct LevelLayout {
    int iconsCount = 0;
    int initStackSize = 0;
    std::vector<PlacedTile> tiles;
};

// Decode the original JindoBlu "Offline Games" Tap Match level blob (the
// `64_tapmatch/bin/levels.bytes` resource). Pure and SDL-free so it is unit
// testable. Tolerant of a short/malformed buffer: it never reads out of range and
// returns the levels it could fully decode.
//
// Format (little-endian), reverse-engineered from libil2cpp:
//   [u32 count][u32 offset for level k at byte 4*k]
//   data section begins at byte (4*count + 4); level k's record is at base+offset.
//   record: [u8 header][u8 tileCount N][N x u16 tile]  (+ an ignored FTUE section)
//   header: iconsCount = h>>3, initStackSize = ((h>>1)&3)+5, hasFTUE = h&1
//   tile u16: layer=u>>10, gx=(u>>6)&0xF, gy=(u>>2)&0xF, xDelta=(u>>1)&1, yDelta=u&1
//   fine coords: x = 2*gx + xDelta, y = 2*gy + yDelta
// FTUE (tutorial-hint) tiles are skipped — the offset table already points us at
// each record, so we never need to walk past its tile list.
[[nodiscard]] std::vector<LevelLayout> parseTapMatchLevels(std::span<const std::uint8_t> bytes);

// The bundled level catalog, loaded once (and cached) from
// assets/tapmatch/levels.bytes next to the executable (via SDL_GetBasePath).
// Empty if the asset is missing/unreadable (the failure is logged). Levels are in
// file order; "level number" N (1-indexed, as shown to the player) is index N-1.
[[nodiscard]] const std::vector<LevelLayout>& tapMatchLevels();

// The catalog is split into 3 difficulty tiers by board complexity (tile count):
// tier 0 (easy) holds the smallest boards, tier 2 (hard) the largest, and each
// tier is ordered easiest-first so a difficulty ramps up as the player advances.
// The original data carries no difficulty tag, so this is derived from the boards.
inline constexpr int kTapMatchTierCount = 3;

// Partition level indices into kTapMatchTierCount complexity tiers (ascending
// tile count), each tier ordered easiest-first. Pure (tapMatchTierLevel uses it on
// the bundled catalog); exposed so the partition can be unit-tested.
[[nodiscard]] std::array<std::vector<int>, kTapMatchTierCount>
tapMatchTierPartition(const std::vector<LevelLayout>& levels);

// Number of boards in `tier` (0..2); 0 if the tier is out of range or the asset
// is missing.
[[nodiscard]] int tapMatchTierSize(int tier);

// The layout for 1-based `level` within `tier`, clamped to the tier's range, or
// nullptr if the tier is empty/out of range.
[[nodiscard]] const LevelLayout* tapMatchTierLevel(int tier, int level);

} // namespace og
