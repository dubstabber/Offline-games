#pragma once

#include "games/arrows/ArrowsBoard.hpp"
#include "games/Difficulty.hpp"

#include <cstdint>

namespace og {

// What kind of board to build: the grid, how long the generator tries to make
// each arrow (in cells), and how much of the grid it fills before stopping.
struct ArrowsLevelSpec {
    int width = 8;
    int height = 12;
    int minLength = 2;
    int maxLength = 6;
    int fillPercent = 55; // stop adding arrows once this share of cells is covered
};

// The spec for a difficulty and 1-based level: each tier has its own grid size
// and density, and levels within a tier pack a little tighter as they go.
[[nodiscard]] ArrowsLevelSpec arrowsLevelSpec(Difficulty difficulty, int level);

// The seed that pins (difficulty, level) to one board, so a level looks the
// same on every device and every retry.
[[nodiscard]] std::uint32_t arrowsLevelSeed(Difficulty difficulty, int level);

// Build a board from a spec and seed. Arrows are added one at a time while
// the builder maintains an order in which they can all be cleared: a new arrow
// slots in right after the last arrow standing in its run to the edge, which
// is allowed only if every arrow whose own run it interrupts is cleared later
// still. So every generated board is solvable (and, since clearing only frees
// cells, never dead-ends). Deterministic: the same spec and seed give the same
// board on every platform.
[[nodiscard]] ArrowsBoard generateArrowsBoard(const ArrowsLevelSpec& spec, std::uint32_t seed);

// Convenience: the board for (difficulty, level).
[[nodiscard]] ArrowsBoard arrowsBoardFor(Difficulty difficulty, int level);

} // namespace og
