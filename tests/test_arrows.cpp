#include "games/arrows/ArrowsBoard.hpp"
#include "games/arrows/ArrowsGenerator.hpp"
#include "games/Difficulty.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

using og::Arrow;
using og::ArrowCell;
using og::ArrowDir;
using og::ArrowsBoard;
using og::ArrowsLevelSpec;
using og::Difficulty;

// A 3x3 board: arrow A runs (0,0)->(1,0) pointing right, straight into the
// single-cell arrow B at (2,0), which points down an empty column.
[[nodiscard]] ArrowsBoard makeSmallBoard() {
    std::vector<Arrow> arrows;
    arrows.push_back(Arrow{.cells = {{0, 0}, {1, 0}}, .dir = ArrowDir::Right});
    arrows.push_back(Arrow{.cells = {{2, 0}}, .dir = ArrowDir::Down});
    return {3, 3, std::move(arrows)};
}

void testOccupancyAndBlocking() {
    const ArrowsBoard board = makeSmallBoard();
    assert(board.arrowCount() == 2);
    assert(board.remaining() == 2);
    assert(board.hearts() == ArrowsBoard::kHearts);
    assert(board.arrowAt(0, 0) == 0 && board.arrowAt(1, 0) == 0);
    assert(board.arrowAt(2, 0) == 1);
    assert(board.arrowAt(1, 1) == -1);
    assert(board.arrowAt(-1, 0) == -1 && board.arrowAt(3, 3) == -1);

    assert(!board.isFree(0));
    assert(board.blockedAfter(0) == 0); // B sits in the very next cell
    assert(board.blockerOf(0) == 1);
    assert(board.isFree(1));
    assert(board.blockedAfter(1) == -1);
    assert(board.blockerOf(1) == -1);
    assert(board.isSolvable());
}

void testTapClearsAndUnblocks() {
    ArrowsBoard board = makeSmallBoard();
    assert(board.tap(0) == ArrowsBoard::Tap::Blocked);
    assert(board.hearts() == ArrowsBoard::kHearts - 1);
    assert(board.remaining() == 2);

    assert(board.tap(1) == ArrowsBoard::Tap::Cleared);
    assert(board.isRemoved(1));
    assert(board.arrowAt(2, 0) == -1);
    assert(board.remaining() == 1);
    assert(board.tap(1) == ArrowsBoard::Tap::Ignored); // already gone

    assert(board.isFree(0)); // the way is clear now
    assert(board.tap(0) == ArrowsBoard::Tap::Cleared);
    assert(board.isWon());
    assert(!board.isLost());
    assert(board.tap(0) == ArrowsBoard::Tap::Ignored);
}

void testRunningOutOfHearts() {
    ArrowsBoard board = makeSmallBoard();
    for (int i = 0; i < ArrowsBoard::kHearts; ++i) {
        assert(!board.isLost());
        assert(board.tap(0) == ArrowsBoard::Tap::Blocked);
    }
    assert(board.hearts() == 0);
    assert(board.isLost());
    assert(board.tap(1) == ArrowsBoard::Tap::Ignored); // the level is over

    board.reset();
    assert(board.hearts() == ArrowsBoard::kHearts);
    assert(board.remaining() == 2);
    assert(board.arrowAt(2, 0) == 1);
    assert(!board.isLost());
}

// A blocked run further away reports how many empty cells precede the blocker.
void testBlockedDistance() {
    std::vector<Arrow> arrows;
    arrows.push_back(Arrow{.cells = {{0, 2}}, .dir = ArrowDir::Right});
    arrows.push_back(Arrow{.cells = {{4, 2}, {4, 1}}, .dir = ArrowDir::Up});
    const ArrowsBoard board(5, 3, std::move(arrows));
    assert(board.blockedAfter(0) == 3);
    assert(board.blockerOf(0) == 1);
    assert(board.isFree(1));
}

// Two arrows pointing at each other can never be cleared.
void testUnsolvableCycle() {
    std::vector<Arrow> arrows;
    arrows.push_back(Arrow{.cells = {{0, 0}}, .dir = ArrowDir::Right});
    arrows.push_back(Arrow{.cells = {{1, 0}}, .dir = ArrowDir::Left});
    ArrowsBoard board(2, 1, std::move(arrows));
    assert(!board.isSolvable());
    assert(board.tap(0) == ArrowsBoard::Tap::Blocked);
    assert(board.tap(1) == ArrowsBoard::Tap::Blocked);
    assert(!board.isSolvable()); // isSolvable never mutates
    assert(board.remaining() == 2);
}

// Every arrow is a simple orthogonal path inside the grid, no two arrows share
// a cell, and the head direction matches the last step of the path.
void checkWellFormed(const ArrowsBoard& board) {
    std::vector<int> owner(static_cast<std::size_t>(board.width() * board.height()), -1);
    for (int i = 0; i < board.arrowCount(); ++i) {
        const Arrow& arrow = board.arrows()[static_cast<std::size_t>(i)];
        assert(!arrow.cells.empty());
        for (std::size_t k = 0; k < arrow.cells.size(); ++k) {
            const ArrowCell& c = arrow.cells[k];
            assert(board.inBounds(c.x, c.y));
            int& slot = owner[static_cast<std::size_t>((c.y * board.width()) + c.x)];
            assert(slot == -1); // no overlaps, no self-crossing
            slot = i;
            if (k > 0) {
                const ArrowCell& p = arrow.cells[k - 1];
                assert(std::abs(c.x - p.x) + std::abs(c.y - p.y) == 1);
            }
        }
        if (arrow.cells.size() >= 2) {
            const ArrowCell& h = arrow.cells.back();
            const ArrowCell& p = arrow.cells[arrow.cells.size() - 2];
            assert(h.x - p.x == og::dirDx(arrow.dir) && h.y - p.y == og::dirDy(arrow.dir));
        }
        assert(board.arrowAt(arrow.cells.front().x, arrow.cells.front().y) == i);
    }
}

[[nodiscard]] int coveredCells(const ArrowsBoard& board) {
    int covered = 0;
    for (const Arrow& arrow : board.arrows()) {
        covered += static_cast<int>(arrow.cells.size());
    }
    return covered;
}

void testGeneratedLevels() {
    constexpr std::array<Difficulty, 3> tiers{Difficulty::Easy, Difficulty::Medium,
                                              Difficulty::Hard};
    for (const Difficulty tier : tiers) {
        for (int level = 1; level <= 30; ++level) {
            const ArrowsBoard board = og::arrowsBoardFor(tier, level);
            const ArrowsLevelSpec spec = og::arrowsLevelSpec(tier, level);
            assert(board.width() == spec.width && board.height() == spec.height);
            checkWellFormed(board);
            assert(board.arrowCount() >= 6);
            // Dense enough to be a puzzle: at least 90% of the requested fill.
            const int cells = board.width() * board.height();
            assert(coveredCells(board) * 100 >= cells * spec.fillPercent * 9 / 10);
            assert(board.isSolvable());
            // Not trivial: some arrow is blocked at the start.
            int blocked = 0;
            for (int i = 0; i < board.arrowCount(); ++i) {
                blocked += board.isFree(i) ? 0 : 1;
            }
            assert(blocked > 0);
        }
    }
}

// The same (difficulty, level) always builds the same board, and different
// levels build different ones.
void testDeterminism() {
    const ArrowsBoard a = og::arrowsBoardFor(Difficulty::Medium, 5);
    const ArrowsBoard b = og::arrowsBoardFor(Difficulty::Medium, 5);
    assert(a.arrowCount() == b.arrowCount());
    for (int i = 0; i < a.arrowCount(); ++i) {
        const Arrow& x = a.arrows()[static_cast<std::size_t>(i)];
        const Arrow& y = b.arrows()[static_cast<std::size_t>(i)];
        assert(x.dir == y.dir && x.cells == y.cells);
    }
    const ArrowsBoard c = og::arrowsBoardFor(Difficulty::Medium, 6);
    bool differs = c.arrowCount() != a.arrowCount();
    for (int i = 0; !differs && i < a.arrowCount(); ++i) {
        differs = a.arrows()[static_cast<std::size_t>(i)].cells !=
                  c.arrows()[static_cast<std::size_t>(i)].cells;
    }
    assert(differs);
    assert(og::arrowsLevelSeed(Difficulty::Easy, 1) != og::arrowsLevelSeed(Difficulty::Hard, 1));
}

// Degenerate specs are clamped rather than crashing.
void testDegenerateSpec() {
    const ArrowsLevelSpec spec{
        .width = 0, .height = -3, .minLength = 0, .maxLength = -1, .fillPercent = 500};
    const ArrowsBoard board = og::generateArrowsBoard(spec, 1);
    assert(board.width() == 1 && board.height() == 1);
    checkWellFormed(board);
    assert(board.isSolvable());
}

} // namespace

int main() {
    testOccupancyAndBlocking();
    testTapClearsAndUnblocks();
    testRunningOutOfHearts();
    testBlockedDistance();
    testUnsolvableCycle();
    testGeneratedLevels();
    testDeterminism();
    testDegenerateSpec();
    std::puts("arrows: all tests passed");
    return 0;
}
