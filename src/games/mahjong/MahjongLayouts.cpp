#include "games/mahjong/MahjongLayouts.hpp"

#include "games/Difficulty.hpp"
#include "games/mahjong/MahjongBoard.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace og {
namespace {

using Slot = MahjongBoard::Slot;

// Small authoring helper: rectangles of tiles and single tiles, in half units.
class Builder {
public:
    Builder& rect(int x, int y, int cols, int rows, int z) {
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                slots_.push_back({.x = x + (2 * c), .y = y + (2 * r), .z = z});
            }
        }
        return *this;
    }
    Builder& tile(int x, int y, int z) {
        slots_.push_back({.x = x, .y = y, .z = z});
        return *this;
    }
    [[nodiscard]] MahjongLayout done(const char* name) {
        return {.name = name, .slots = std::move(slots_)};
    }

private:
    std::vector<Slot> slots_;
};

// ---- Easy: 38-60 tiles, 2-3 layers ------------------------------------------
[[nodiscard]] std::vector<MahjongLayout> easyLayouts() {
    std::vector<MahjongLayout> out;
    // 56: a 6x6 block with a 4x4 and a 2x2 stacked on it.
    out.push_back(
        Builder().rect(0, 0, 6, 6, 0).rect(2, 2, 4, 4, 1).rect(4, 4, 2, 2, 2).done("Pyramid"));
    // 60: three wide bars, each carrying a 2x2 on its middle.
    out.push_back(Builder()
                      .rect(0, 0, 8, 2, 0)
                      .rect(0, 4, 8, 2, 0)
                      .rect(0, 8, 8, 2, 0)
                      .rect(3, 0, 2, 2, 1)
                      .rect(3, 4, 2, 2, 1)
                      .rect(3, 8, 2, 2, 1)
                      .done("Bars"));
    // 48: rows widening to the middle and back, a 3x2 and a half-offset pair on top.
    {
        Builder b;
        constexpr std::array<int, 8> widths{2, 4, 6, 8, 8, 6, 4, 2};
        for (std::size_t r = 0; r < widths.size(); ++r) {
            b.rect(8 - widths.at(r), 2 * static_cast<int>(r), widths.at(r), 1, 0);
        }
        b.rect(5, 4, 3, 2, 1).rect(6, 5, 2, 1, 2);
        out.push_back(std::move(b).done("Diamond"));
    }
    // 38: a hollow 6x6 ring with two loose tiles inside and raised corners.
    {
        Builder b;
        for (int r = 0; r < 6; ++r) {
            for (int c = 0; c < 6; ++c) {
                if (c >= 2 && c <= 3 && r >= 2 && r <= 3) {
                    continue;
                }
                b.tile(2 * c, 2 * r, 0);
            }
        }
        b.tile(5, 4, 0).tile(5, 6, 0);
        b.tile(0, 0, 1).tile(10, 0, 1).tile(0, 10, 1).tile(10, 10, 1);
        out.push_back(std::move(b).done("Ring"));
    }
    return out;
}

// ---- Medium: 80-84 tiles, 3-4 layers ----------------------------------------
[[nodiscard]] std::vector<MahjongLayout> mediumLayouts() {
    std::vector<MahjongLayout> out;
    // 84: an 8x8 block, a 4x4 on it, a half-offset 2x2 on top.
    out.push_back(
        Builder().rect(0, 0, 8, 8, 0).rect(2, 2, 4, 4, 1).rect(3, 3, 2, 2, 2).done("Fortress"));
    // 84: two 3x10 towers joined by a low bridge, with spines and peaks.
    out.push_back(Builder()
                      .rect(0, 0, 3, 10, 0)
                      .rect(14, 0, 3, 10, 0)
                      .rect(6, 8, 4, 2, 0)
                      .rect(1, 2, 1, 6, 1)
                      .rect(15, 2, 1, 6, 1)
                      .rect(8, 8, 2, 1, 1)
                      .tile(1, 7, 2)
                      .tile(15, 7, 2)
                      .done("Towers"));
    // 80: an 8x10 ring around a 2x3 block, corners and the block raised.
    {
        Builder b;
        for (int r = 0; r < 10; ++r) {
            for (int c = 0; c < 8; ++c) {
                if (c >= 2 && c <= 5 && r >= 2 && r <= 7) {
                    continue;
                }
                b.tile(2 * c, 2 * r, 0);
            }
        }
        b.rect(5, 7, 2, 3, 0);
        b.rect(0, 0, 2, 2, 1).rect(12, 0, 2, 2, 1).rect(0, 16, 2, 2, 1).rect(12, 16, 2, 2, 1);
        b.tile(6, 8, 1).tile(6, 10, 1);
        out.push_back(std::move(b).done("Arena"));
    }
    return out;
}

// ---- Hard: the full 144-tile set, 2-5 layers --------------------------------
[[nodiscard]] std::vector<MahjongLayout> hardLayouts() {
    std::vector<MahjongLayout> out;
    // 144: the classic turtle stood upright — columns of 12/8/10/12/12/10/8/12,
    // a loose tile on the left, two stacked on the right, and a 6x6, 4x4,
    // 2x2 and single tile stacked in the middle.
    {
        Builder b;
        constexpr std::array<int, 8> heights{12, 8, 10, 12, 12, 10, 8, 12};
        for (std::size_t c = 0; c < heights.size(); ++c) {
            b.rect(2 + (2 * static_cast<int>(c)), 12 - heights.at(c), 1, heights.at(c), 0);
        }
        b.tile(0, 11, 0).tile(18, 11, 0).tile(18, 11, 1);
        b.rect(4, 6, 6, 6, 1).rect(6, 8, 4, 4, 2).rect(8, 10, 2, 2, 3).tile(9, 11, 4);
        out.push_back(std::move(b).done("Turtle"));
    }
    // 144: a solid 10x12 floor with a half-offset 4x6 keep on it.
    out.push_back(Builder().rect(0, 0, 10, 12, 0).rect(3, 3, 4, 6, 1).done("Castle"));
    // 144: two 4x12 towers, a bridge between them, spines and ridges on top.
    out.push_back(Builder()
                      .rect(0, 0, 4, 12, 0)
                      .rect(12, 0, 4, 12, 0)
                      .rect(8, 4, 2, 4, 0)
                      .rect(1, 2, 2, 8, 1)
                      .rect(13, 2, 2, 8, 1)
                      .rect(2, 4, 1, 4, 2)
                      .rect(14, 4, 1, 4, 2)
                      .done("Bridge"));
    return out;
}

// A 32-bit integer hash (the lowbias32 mix) so neighbouring levels land on
// unrelated seeds.
[[nodiscard]] std::uint32_t mix32(std::uint32_t x) {
    x ^= x >> 16U;
    x *= 0x7feb352dU;
    x ^= x >> 15U;
    x *= 0x846ca68bU;
    x ^= x >> 16U;
    return x;
}

} // namespace

std::span<const MahjongLayout> mahjongLayouts(Difficulty difficulty) {
    static const std::vector<MahjongLayout> easy = easyLayouts();
    static const std::vector<MahjongLayout> medium = mediumLayouts();
    static const std::vector<MahjongLayout> hard = hardLayouts();
    switch (difficulty) {
    case Difficulty::Easy:
        return easy;
    case Difficulty::Medium:
        return medium;
    case Difficulty::Hard:
    case Difficulty::VeryHard:
        return hard;
    }
    return easy;
}

const MahjongLayout& mahjongLayoutFor(Difficulty difficulty, int level) {
    const std::span<const MahjongLayout> layouts = mahjongLayouts(difficulty);
    const auto index = static_cast<std::size_t>(std::max(0, level - 1)) % layouts.size();
    return layouts.subspan(index).front();
}

std::uint32_t mahjongLevelSeed(Difficulty difficulty, int level) {
    const auto tier = static_cast<std::uint32_t>(difficulty);
    return mix32(0x4D414A4FU ^ (tier << 24U) ^ static_cast<std::uint32_t>(std::max(1, level)));
}

} // namespace og
