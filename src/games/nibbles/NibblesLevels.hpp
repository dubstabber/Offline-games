#pragma once

#include "games/nibbles/NibblesTypes.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace og::nibbles {

// Decode assets/nibbles/levels.bytes. Pure and SDL-free so the parser and the
// real bundled asset can be tested without opening a window.
//
// Format (little-endian):
//   header: u32 magic "NIBB", u16 version=1, u16 levelCount
//   record: u8 width, u8 height, u8 sourceLevel, u8 spawnCount, u8 warpCount
//           width*height u8 cells: 0 empty, 1 wall, 2 warp
//           spawnCount x (u8 x, u8 y, u8 direction 0 up/1 right/2 down/3 left)
//           warpCount x (u8 id, u8 sourceX, u8 sourceY, u8 targetX, u8 targetY,
//                        u8 flags: bit0 random, bit1 bidirectional)
[[nodiscard]] std::vector<NibblesLevel> parseNibblesLevels(std::span<const std::uint8_t> bytes);

[[nodiscard]] const std::vector<NibblesLevel>& nibblesLevels();
[[nodiscard]] int nibblesLevelCount();
[[nodiscard]] const NibblesLevel* nibblesLevel(int level);

} // namespace og::nibbles
