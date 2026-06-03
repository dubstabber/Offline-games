#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace og {

// One Block Fill board: a width x height grid where some positions are holes.
// `playable` is row-major (size width*height); 1 = a playable cell, 0 = a hole.
// The rope starts on (startX, startY) — always a playable cell — and must cover
// every playable cell exactly once. Decoded from the original game's level data.
struct BlockFillLevel {
    int width = 0;
    int height = 0;
    int startX = 0;
    int startY = 0;
    std::vector<std::uint8_t> playable; // row-major, 1 = playable, 0 = hole

    [[nodiscard]] int playableCount() const;
};

// Block Fill ships four difficulty tiers (Easy, Medium, Hard, Very Hard); the
// original calls the fourth "Extra Hard". They map onto Difficulty 0..3.
inline constexpr int kBlockFillTierCount = 4;

// Decode the bundled assets/blockfill/levels.bytes blob (produced offline by
// tools/extract_blockfill_levels.py from the original boards). Pure and SDL-free
// so it is unit testable. Tolerant of a short/malformed buffer: it never reads
// out of range and returns the tiers it could fully decode.
//
// Format (little-endian):
//   header (16 bytes): u32 magic "BLKF", u16 version, u16 tierCount=4,
//                      u16 count[0..3] (one per tier)
//   then every level, tier order (tier 0 first), ascending difficulty within a
//   tier; each record: u8 w, u8 h, u8 startX, u8 startY, then ceil(w*h/8) bytes
//   of a row-major LSB-first missing-bitmask (bit set = hole; index = y*w + x).
[[nodiscard]] std::array<std::vector<BlockFillLevel>, kBlockFillTierCount>
parseBlockFillLevels(std::span<const std::uint8_t> bytes);

// The bundled tiers, loaded once (and cached) from assets/blockfill/levels.bytes
// next to the executable (via SDL_GetBasePath). All tiers are empty if the asset
// is missing/unreadable (the failure is logged).
[[nodiscard]] const std::array<std::vector<BlockFillLevel>, kBlockFillTierCount>& blockFillLevels();

// Number of boards in `tier` (0..3); 0 if the tier is out of range or empty.
[[nodiscard]] int blockFillTierSize(int tier);

// The layout for 1-based `level` within `tier`, clamped to the tier's range, or
// nullptr if the tier is empty/out of range.
[[nodiscard]] const BlockFillLevel* blockFillTierLevel(int tier, int level);

} // namespace og
