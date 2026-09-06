#pragma once

#include "games/Difficulty.hpp"
#include "games/mahjong/MahjongBoard.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace og {

// A named tile arrangement. Positions are in half-tile units (a tile covers
// 2x2) with layer 0 on the table; every layout fits a portrait board of at
// most 10 x 12 tiles, and has an even tile count.
struct MahjongLayout {
    const char* name = "";
    std::vector<MahjongBoard::Slot> slots;
};

// The layouts of a difficulty: a handful per tier, growing from a few dozen
// tiles on two or three layers (Easy) to the full 144-tile set (Hard).
[[nodiscard]] std::span<const MahjongLayout> mahjongLayouts(Difficulty difficulty);

// Levels cycle through the tier's layouts; the seed pins the deal so a level
// looks the same on every retry and every device.
[[nodiscard]] const MahjongLayout& mahjongLayoutFor(Difficulty difficulty, int level);
[[nodiscard]] std::uint32_t mahjongLevelSeed(Difficulty difficulty, int level);

} // namespace og
