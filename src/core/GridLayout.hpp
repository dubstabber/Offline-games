#pragma once

#include <algorithm> // std::min

// Pure, SDL-free helpers for placing a uniform cell grid inside a fixed play
// area and mapping a pointer back to a cell. Header-only (no CMake entry) and
// unit-tested. Block Fill, Minesweeper and Tap Match all share the same "largest
// capped square cell, centered in the area" fit and the same cell hit test; this
// is that logic in one place.
namespace og::grid {

// A fitted, centered grid placement: `cellPx` is the largest square cell (capped
// at maxCell) that fits gridW x gridH into the areaW x areaH box, and
// (originX, originY) is the top-left that centers the resulting grid in the box.
struct Fit {
    float cellPx = 0.0F;
    float originX = 0.0F;
    float originY = 0.0F;
};

// Equivalent to cellPx = min(areaW/gridW, areaH/gridH, maxCell) with a centered
// origin (the formula every grid game open-codes today). gridW/gridH below 1 are
// treated as 1 so an empty board never divides by zero.
[[nodiscard]] constexpr Fit fitCentered(float areaX, float areaY, float areaW, float areaH,
                                        int gridW, int gridH, float maxCell) {
    const auto gw = static_cast<float>(gridW < 1 ? 1 : gridW);
    const auto gh = static_cast<float>(gridH < 1 ? 1 : gridH);
    const float cell = std::min({areaW / gw, areaH / gh, maxCell});
    return {.cellPx = cell,
            .originX = areaX + ((areaW - (gw * cell)) / 2.0F),
            .originY = areaY + ((areaH - (gh * cell)) / 2.0F)};
}

// Column (x) and row (y) of the cell containing pixel (px, py). Returns false
// when the pixel is left of or above the origin (matching the scenes' existing
// `px < originX || py < originY` guard); callers still range-check col/row
// against their own grid extent and playability.
[[nodiscard]] constexpr bool cellAt(const Fit& fit, float px, float py, int& col, int& row) {
    if (px < fit.originX || py < fit.originY) {
        return false;
    }
    col = static_cast<int>((px - fit.originX) / fit.cellPx);
    row = static_cast<int>((py - fit.originY) / fit.cellPx);
    return true;
}

} // namespace og::grid
