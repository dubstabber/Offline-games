#pragma once

#include "games/hexanaut/HexTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace og::hexanaut {

// One board cell. Four bytes, so even a few-thousand-cell board is a handful of
// KB and stays cache-resident.
struct Cell {
    PlayerId owner = kNeutral;      // territory owner, or kNeutral
    PlayerId trailOwner = kNoTrail; // active trail laid here, or kNoTrail
    std::uint8_t flags = 0;         // bit0: powerup present, bit1: flood-fill visited scratch
    std::uint8_t powerup = 0;       // PowerUp type (0 = none)
};

// A dense, bounded flat-top hex board. Valid coords are q in [0,width) and
// r in [0,height); the axial->world shear renders this as a rhombus arena.
// Dense storage (not a hash map) keeps the hot owner/trail queries
// cache-friendly and gives the flood-fill a finite domain.
class HexGrid {
public:
    HexGrid(int width, int height);

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] int cellCount() const { return static_cast<int>(cells_.size()); }

    [[nodiscard]] bool contains(HexCoord c) const {
        return c.q >= 0 && c.q < width_ && c.r >= 0 && c.r < height_;
    }
    [[nodiscard]] int index(HexCoord c) const { return (c.r * width_) + c.q; }
    [[nodiscard]] HexCoord fromIndex(int i) const { return {i % width_, i / width_}; }

    [[nodiscard]] Cell& at(HexCoord c) { return cells_.at(static_cast<std::size_t>(index(c))); }
    [[nodiscard]] const Cell& at(HexCoord c) const {
        return cells_.at(static_cast<std::size_t>(index(c)));
    }

    [[nodiscard]] std::vector<Cell>& cells() { return cells_; }
    [[nodiscard]] const std::vector<Cell>& cells() const { return cells_; }

private:
    int width_;
    int height_;
    std::vector<Cell> cells_;
};

} // namespace og::hexanaut
