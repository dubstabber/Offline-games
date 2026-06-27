#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace og {

struct SokobanLevel {
    int width = 0;
    int height = 0;
    int sourceSet = 0;
    int sourceLevel = 0; // 1-based level number inside the original source set
    std::vector<char> cells;

    [[nodiscard]] int boxCount() const;
    [[nodiscard]] int goalCount() const;
};

inline constexpr int kSokobanTierCount = 3;

// Decode assets/sokoban/levels.bytes. Pure and SDL-free so the parser and real
// asset can be unit-tested.
//
// Format (little-endian):
//   header: u32 magic "SOKO", u16 version=1, u16 tierCount=3, u16 count[0..2]
//   record: u8 width, u8 height, u8 sourceSet, u16 sourceLevel1Based,
//           then width*height cell bytes using standard Sokoban chars.
[[nodiscard]] std::array<std::vector<SokobanLevel>, kSokobanTierCount>
parseSokobanLevels(std::span<const std::uint8_t> bytes);

[[nodiscard]] const std::array<std::vector<SokobanLevel>, kSokobanTierCount>& sokobanLevels();
[[nodiscard]] int sokobanTierSize(int tier);
[[nodiscard]] const SokobanLevel* sokobanTierLevel(int tier, int level);
[[nodiscard]] std::string_view sokobanSetName(int sourceSet);

} // namespace og
