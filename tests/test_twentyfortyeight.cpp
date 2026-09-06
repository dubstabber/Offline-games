#include "games/twentyfortyeight/TwentyFortyEightBoard.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using og::TwentyFortyEightBoard;
using og::twentyFortyEightParams;
using Direction = TwentyFortyEightBoard::Direction;
using MoveResult = TwentyFortyEightBoard::MoveResult;
using Rows = std::vector<std::vector<int>>;

// A square board with the given row values laid out top-to-bottom (0 = empty).
TwentyFortyEightBoard boardFrom(const Rows& rows, std::uint32_t seed = 1) {
    TwentyFortyEightBoard board(static_cast<int>(rows.size()), seed);
    board.clearForTest();
    for (int y = 0; y < board.size(); ++y) {
        for (int x = 0; x < board.size(); ++x) {
            board.setForTest(x, y,
                             rows.at(static_cast<std::size_t>(y)).at(static_cast<std::size_t>(x)));
        }
    }
    return board;
}

// Assert the board matches `rows`, allowing the one tile spawned by `result` to
// occupy a cell the layout expects empty.
void expectBoard(const TwentyFortyEightBoard& board, const Rows& rows, const MoveResult& result) {
    for (int y = 0; y < board.size(); ++y) {
        for (int x = 0; x < board.size(); ++x) {
            const int expected =
                rows.at(static_cast<std::size_t>(y)).at(static_cast<std::size_t>(x));
            if (x == result.spawnX && y == result.spawnY) {
                assert(expected == 0); // spawns only land on empty cells
                assert(board.at(x, y) == result.spawnValue);
            } else {
                assert(board.at(x, y) == expected);
            }
        }
    }
}

int tileCount(const TwentyFortyEightBoard& board) {
    return (board.size() * board.size()) - board.emptyCount();
}

std::vector<int> row(const TwentyFortyEightBoard& board, int y) {
    std::vector<int> out;
    out.reserve(static_cast<std::size_t>(board.size()));
    for (int x = 0; x < board.size(); ++x) {
        out.push_back(board.at(x, y));
    }
    return out;
}

// Difficulty maps to board size and goal: bigger boards are easier.
void testParams() {
    assert(twentyFortyEightParams(0).size == 5 && twentyFortyEightParams(0).goal == 2048);
    assert(twentyFortyEightParams(1).size == 4 && twentyFortyEightParams(1).goal == 2048);
    assert(twentyFortyEightParams(2).size == 3 && twentyFortyEightParams(2).goal == 1024);
}

// A fresh board has exactly two tiles, each a 2 or a 4, and no score.
void testStart() {
    for (const int size : {3, 4, 5}) {
        const TwentyFortyEightBoard board(size, 7);
        assert(board.size() == size);
        assert(tileCount(board) == 2);
        assert(board.score() == 0);
        assert(!board.canUndo());
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const int v = board.at(x, y);
                assert(v == 0 || v == 2 || v == 4);
            }
        }
    }
}

// Sliding left packs tiles against the wall and merges each equal pair once:
// [2,2,2,2] -> [4,4], never [8]; [2,2,4] -> [4,4], never [8].
void testMergeOncePerMove() {
    TwentyFortyEightBoard board =
        boardFrom({{2, 2, 2, 2}, {2, 2, 4, 0}, {4, 2, 2, 0}, {2, 0, 2, 0}});
    MoveResult result;
    assert(board.move(Direction::Left, &result));
    // Row 0 = [4,4] (+8), row 1 = [4,4] (+4), row 2 = [4,4] (+4), row 3 = [4] (+4).
    expectBoard(board, {{4, 4, 0, 0}, {4, 4, 0, 0}, {4, 4, 0, 0}, {4, 0, 0, 0}}, result);
    assert(result.gained == 8 + 4 + 4 + 4);
    assert(board.score() == 20);
    // Exactly one tile spawned, on a cell that was empty after the slide.
    assert(result.spawnX >= 0 && result.spawnY >= 0);
    assert(result.spawnValue == 2 || result.spawnValue == 4);
    assert(board.at(result.spawnX, result.spawnY) == result.spawnValue);
    assert(tileCount(board) == 7 + 1);
}

// All four directions slide toward their own wall.
void testDirections() {
    MoveResult result;
    {
        TwentyFortyEightBoard b =
            boardFrom({{0, 0, 0, 2}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}});
        assert(b.move(Direction::Left, &result));
        expectBoard(b, {{2, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, result);
    }
    {
        TwentyFortyEightBoard b =
            boardFrom({{2, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}});
        assert(b.move(Direction::Right, &result));
        expectBoard(b, {{0, 0, 0, 2}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, result);
    }
    {
        TwentyFortyEightBoard b =
            boardFrom({{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 2, 0, 0}});
        assert(b.move(Direction::Up, &result));
        expectBoard(b, {{0, 2, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, result);
    }
    {
        TwentyFortyEightBoard b =
            boardFrom({{0, 2, 0, 0}, {0, 2, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}});
        assert(b.move(Direction::Down, &result));
        expectBoard(b, {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 4, 0, 0}}, result);
        assert(b.score() == 4);
    }
}

// A move that changes nothing is rejected: no spawn, no score, no undo entry.
void testNoOpMoveRejected() {
    TwentyFortyEightBoard board =
        boardFrom({{2, 4, 0, 0}, {8, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}});
    MoveResult result;
    assert(!board.move(Direction::Left, &result));
    assert(result.tiles.empty() && result.spawnX == -1);
    assert(tileCount(board) == 3);
    assert(board.score() == 0);
    assert(!board.canUndo());
    assert(row(board, 0) == std::vector<int>({2, 4, 0, 0}));
}

// The move record tells the scene where every tile went and which ones merged.
void testMoveRecord() {
    TwentyFortyEightBoard board =
        boardFrom({{0, 2, 0, 2}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}});
    MoveResult result;
    assert(board.move(Direction::Left, &result));
    assert(result.tiles.size() == 2);
    for (const auto& t : result.tiles) {
        assert(t.toX == 0 && t.toY == 0); // both land on the merged cell
        assert(t.value == 2 && t.merged);
        assert(t.fromY == 0 && (t.fromX == 1 || t.fromX == 3));
    }
    assert(board.at(0, 0) == 4);
}

// Undo restores the pre-move board and score (and removes the spawned tile);
// it works once per move.
void testUndo() {
    TwentyFortyEightBoard board =
        boardFrom({{2, 2, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}});
    assert(!board.undo());
    assert(board.move(Direction::Left));
    assert(board.score() == 4 && board.canUndo());
    assert(board.undo());
    assert(board.score() == 0);
    assert(row(board, 0) == std::vector<int>({2, 2, 0, 0}));
    assert(tileCount(board) == 2);
    assert(!board.canUndo());
    assert(!board.undo()); // only one step is kept
}

// Game over = full board with no equal neighbors; a single mergeable pair keeps
// the game alive even with every cell filled.
void testGameOverDetection() {
    const TwentyFortyEightBoard stuck =
        boardFrom({{2, 4, 2, 4}, {4, 2, 4, 2}, {2, 4, 2, 4}, {4, 2, 4, 2}});
    assert(!stuck.canMove());
    TwentyFortyEightBoard alive =
        boardFrom({{2, 4, 2, 4}, {4, 2, 4, 2}, {2, 4, 2, 4}, {4, 2, 4, 4}});
    assert(alive.canMove());
    assert(alive.move(Direction::Right));
    assert(alive.at(3, 3) == 8);
}

// maxTile tracks the biggest tile (the win check); a 3x3 board's tiles fit.
void testMaxTileAndSmallBoard() {
    TwentyFortyEightBoard board = boardFrom({{1024, 512, 0}, {0, 0, 0}, {0, 0, 0}});
    assert(board.size() == 3);
    assert(board.maxTile() == 1024);
    MoveResult result;
    assert(board.move(Direction::Right, &result));
    expectBoard(board, {{0, 1024, 512}, {0, 0, 0}, {0, 0, 0}}, result);
}

// Same size + seed + moves => identical boards (the scene relies on the RNG
// being the only source of randomness).
void testDeterminism() {
    TwentyFortyEightBoard a(4, 99);
    TwentyFortyEightBoard b(4, 99);
    const std::vector<Direction> seq{Direction::Left, Direction::Up, Direction::Right,
                                     Direction::Down};
    for (std::size_t k = 0; k < 40; ++k) {
        const Direction d = seq.at(k % seq.size());
        const bool ma = a.move(d);
        const bool mb = b.move(d);
        assert(ma == mb);
    }
    assert(a.score() == b.score());
    for (int y = 0; y < 4; ++y) {
        assert(row(a, y) == row(b, y));
    }
}

// reset() starts over on the same size with two tiles and zero score.
void testReset() {
    TwentyFortyEightBoard board(5, 3);
    for (int k = 0; k < 10; ++k) {
        board.move(static_cast<Direction>(k % 4));
    }
    board.reset();
    assert(board.size() == 5);
    assert(tileCount(board) == 2);
    assert(board.score() == 0);
    assert(!board.canUndo());
}

} // namespace

int main() {
    testParams();
    testStart();
    testMergeOncePerMove();
    testDirections();
    testNoOpMoveRejected();
    testMoveRecord();
    testUndo();
    testGameOverDetection();
    testMaxTileAndSmallBoard();
    testDeterminism();
    testReset();
    std::puts("All 2048 tests passed.");
    return 0;
}
