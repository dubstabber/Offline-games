#include "games/minesweeper/MineSweeperBoard.hpp"
#include "games/minesweeper/MineSweeperSolver.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using og::MineSweeperBoard;
using State = og::MineSweeperBoard::State;

// A layout (row-major) with mines at the given indices.
std::vector<std::uint8_t> layoutWith(int width, int height, std::initializer_list<int> mines) {
    std::vector<std::uint8_t> layout(static_cast<std::size_t>(width * height), 0);
    for (const int i : mines) {
        layout.at(static_cast<std::size_t>(i)) = 1;
    }
    return layout;
}

// Adjacency is computed when a layout is placed: a center mine makes every
// surrounding cell a "1".
void testAdjacency() {
    MineSweeperBoard board(3, 3, 1, 1);
    board.setLayout(layoutWith(3, 3, {4})); // mine at center (r1,c1)
    assert(board.started());
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            if (r == 1 && c == 1) {
                assert(board.at(r, c).mine);
            } else {
                assert(board.at(r, c).adjacent == 1);
            }
        }
    }
}

// Revealing a zero-cell floods the whole connected safe region; with a single
// mine that clears every safe cell and wins (remaining mines auto-flag to zero).
void testFloodAndWin() {
    MineSweeperBoard board(3, 3, 1, 1);
    board.setLayout(layoutWith(3, 3, {0})); // mine at corner (r0,c0)
    board.reveal(2, 2);
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            const bool isMineCell = r == 0 && c == 0;
            assert(board.at(r, c).revealed != isMineCell);
        }
    }
    assert(board.state() == State::Won);
    assert(board.minesRemaining() == 0); // the lone mine was auto-flagged on win
}

// Revealing a mine loses, marks it exploded, and uncovers every mine.
void testRevealMineLoses() {
    MineSweeperBoard board(3, 3, 1, 1);
    board.setLayout(layoutWith(3, 3, {0}));
    board.reveal(0, 0);
    assert(board.state() == State::Lost);
    assert(board.at(0, 0).exploded);
    assert(board.at(0, 0).revealed);
}

// Flagging toggles, decrements the mines-left counter, and blocks reveals.
void testFlagging() {
    MineSweeperBoard board(3, 3, 1, 1);
    board.setLayout(layoutWith(3, 3, {0}));
    assert(board.minesRemaining() == 1);
    board.toggleFlag(0, 0);
    assert(board.at(0, 0).flagged);
    assert(board.minesRemaining() == 0);
    board.reveal(0, 0); // flagged cell ignores reveal
    assert(board.state() == State::Playing);
    assert(!board.at(0, 0).revealed);
    board.toggleFlag(0, 0);
    assert(!board.at(0, 0).flagged);
    assert(board.minesRemaining() == 1);
}

// The first reveal generates the layout: the tapped cell and its neighbours are
// guaranteed mine-free, so it always opens a cascade.
void testFirstRevealSafe() {
    MineSweeperBoard board(9, 10, 15, 0xC0FFEEU);
    board.reveal(5, 4);
    assert(board.started());
    assert(board.at(5, 4).revealed);
    assert(!board.at(5, 4).mine);
    assert(board.at(5, 4).adjacent == 0); // neighbours safe => opening is a zero
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            assert(!board.at(5 + dr, 4 + dc).mine);
        }
    }
}

// Chording a satisfied number reveals its remaining neighbours; here that clears
// the board.
void testChordClears() {
    MineSweeperBoard board(3, 3, 1, 1);
    board.setLayout(layoutWith(3, 3, {0})); // mine at (0,0)
    board.reveal(1, 1);                     // a "1" (touches the corner mine)
    assert(board.at(1, 1).adjacent == 1);
    board.toggleFlag(0, 0); // flag the mine
    board.chord(1, 1);
    assert(board.state() == State::Won);
}

// A wrong flag makes chording detonate the real mine.
void testChordWrongFlagLoses() {
    MineSweeperBoard board(3, 3, 1, 1);
    board.setLayout(layoutWith(3, 3, {0})); // mine at (0,0)
    board.reveal(1, 1);
    board.toggleFlag(2, 2); // flag a safe cell (still a neighbour of (1,1))
    board.chord(1, 1);      // satisfied by the (wrong) flag -> opens the mine
    assert(board.state() == State::Lost);
}

// A single mine means the opening floods the entire board, so any board is
// solvable by logic alone.
void testSolverTrivial() {
    const std::vector<std::uint8_t> layout = layoutWith(4, 4, {0});
    assert(og::isMineLayoutSolvable(4, 4, layout, 3, 3));
    // An opening on a mine is never valid.
    assert(!og::isMineLayoutSolvable(4, 4, layout, 0, 0));
}

// The generator only returns solvable layouts that keep the opening safe, and is
// deterministic for a fixed seed.
void testGenerateSolvable() {
    constexpr int kW = 9;
    constexpr int kH = 10;
    constexpr int kMines = 15;
    constexpr int kR = 5;
    constexpr int kC = 4;
    const std::vector<std::uint8_t> layout =
        og::generateSolvableMines(kW, kH, kMines, kR, kC, 0x1234U);

    int count = 0;
    for (const std::uint8_t v : layout) {
        count += (v != 0) ? 1 : 0;
    }
    assert(count == kMines);
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            assert(layout.at(static_cast<std::size_t>(((kR + dr) * kW) + (kC + dc))) == 0);
        }
    }
    assert(og::isMineLayoutSolvable(kW, kH, layout, kR, kC));

    const std::vector<std::uint8_t> again =
        og::generateSolvableMines(kW, kH, kMines, kR, kC, 0x1234U);
    assert(again == layout); // same seed -> same board
}

// Every difficulty's real config produces solvable boards across many seeds.
void testEveryDifficultySolvable() {
    struct Cfg {
        int w;
        int h;
        int mines;
    };
    for (const Cfg cfg : {Cfg{.w = 8, .h = 8, .mines = 8}, Cfg{.w = 9, .h = 10, .mines = 15},
                          Cfg{.w = 9, .h = 14, .mines = 27}}) {
        for (std::uint32_t seed = 1; seed <= 40; ++seed) {
            const std::vector<std::uint8_t> layout =
                og::generateSolvableMines(cfg.w, cfg.h, cfg.mines, cfg.h / 2, cfg.w / 2, seed);
            assert(og::isMineLayoutSolvable(cfg.w, cfg.h, layout, cfg.h / 2, cfg.w / 2));
        }
    }
}

} // namespace

int main() {
    testAdjacency();
    testFloodAndWin();
    testRevealMineLoses();
    testFlagging();
    testFirstRevealSafe();
    testChordClears();
    testChordWrongFlagLoses();
    testSolverTrivial();
    testGenerateSolvable();
    testEveryDifficultySolvable();
    std::puts("All MineSweeper tests passed.");
    return 0;
}
