#pragma once

#include <cstdint>
#include <vector>

namespace og {

// Pure, SDL-free Minesweeper logic solver and board generator. Both work on a
// raw mine layout (row-major, `layout[r * width + c] != 0` marks a mine), so they
// have no dependency on MineSweeperBoard and are independently unit-testable.

// True iff, starting from revealing `(r0, c0)`, the board can be fully cleared
// using only *forced* logical deductions — never a guess. This is the guarantee
// behind the difficulty screen's "always solvable using logic" promise: the
// generator below keeps only layouts this returns true for.
//
// The deductions are sound (every one is implied by the true layout), so the
// solver never reveals a mine; a board it cannot fully clear is reported
// unsolvable (conservatively) rather than guessed at.
[[nodiscard]] bool isMineLayoutSolvable(int width, int height,
                                        const std::vector<std::uint8_t>& layout, int r0, int c0);

// Build a layout of `mineCount` mines over `width * height` cells such that
// `(r0, c0)` and its eight neighbours are mine-free (so the first tap opens a
// cascade) and the board is logically solvable from `(r0, c0)`. Tries random
// layouts seeded by `seed` until one passes; on the (pathological) attempt-cap
// miss it returns the last layout so play can always start. `seed` makes
// generation deterministic for tests.
[[nodiscard]] std::vector<std::uint8_t> generateSolvableMines(int width, int height, int mineCount,
                                                              int r0, int c0, std::uint32_t seed);

} // namespace og
