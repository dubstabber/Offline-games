#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace og {

// Per-difficulty rules for 2048: the board edge length and the tile that counts
// as winning. Smaller boards are harder — a 3x3 board can never hold more than a
// 1024, so Hard's goal is that.
struct TwentyFortyEightParams {
    int size;
    int goal;
};

[[nodiscard]] constexpr TwentyFortyEightParams twentyFortyEightParams(int difficultyIndex) {
    switch (difficultyIndex) {
    case 0:
        return {.size = 5, .goal = 2048};
    case 2:
        return {.size = 3, .goal = 1024};
    default:
        return {.size = 4, .goal = 2048};
    }
}

// The pure, SDL-free 2048 board. A move slides every tile as far as it goes in
// one direction; two equal tiles that collide merge into their sum (each tile
// merges at most once per move) and score that sum. After any move that changed
// the board a new 2 (or, 10% of the time, a 4) appears on a random empty cell.
// The game is lost when no move can change the board. One step of undo is kept.
// Determinism comes from the seeded RNG, so (size, seed, moves) replays exactly.
class TwentyFortyEightBoard {
public:
    enum class Direction : std::uint8_t { Up, Down, Left, Right };

    static constexpr int kMinSize = 3;
    static constexpr int kMaxSize = 6;
    static constexpr int kMaxCells = kMaxSize * kMaxSize;

    // One pre-move tile and where it ended up. Two tiles that merged share a
    // destination and are both flagged `merged`; `value` is the tile's value
    // BEFORE the merge. The scene slides these, then pops the merged results.
    struct TileMove {
        int fromX = 0;
        int fromY = 0;
        int toX = 0;
        int toY = 0;
        int value = 0;
        bool merged = false;
    };

    // Everything the scene needs to animate one move.
    struct MoveResult {
        std::vector<TileMove> tiles;
        int spawnX = -1; // the tile that appeared afterwards (-1: none)
        int spawnY = -1;
        int spawnValue = 0;
        int gained = 0; // score earned by this move's merges
    };

    TwentyFortyEightBoard(int size, std::uint32_t seed);

    [[nodiscard]] int size() const { return size_; }
    [[nodiscard]] int at(int x, int y) const { return cells_.at(index(x, y)); }
    [[nodiscard]] int score() const { return score_; }
    [[nodiscard]] int maxTile() const;
    [[nodiscard]] int emptyCount() const;
    // True while some direction would still change the board.
    [[nodiscard]] bool canMove() const;
    [[nodiscard]] bool canUndo() const { return canUndo_; }

    // Slide + merge toward `dir`, then spawn one tile. Returns false — leaving the
    // board, score and undo state untouched and spawning nothing — if nothing
    // would move. `out` (optional) receives the tile motion for animation.
    bool move(Direction dir, MoveResult* out = nullptr);
    // Restore the board and score from before the last successful move (one
    // step only). False if there is nothing to undo.
    bool undo();
    // A fresh game on the same board size: empty except for two starting tiles.
    void reset();

    // ---- Test seams ----------------------------------------------------------
    void clearForTest();
    void setForTest(int x, int y, int value) { cells_.at(index(x, y)) = value; }

private:
    using Cells = std::array<int, kMaxCells>;
    struct Pos {
        int x = 0;
        int y = 0;
    };

    [[nodiscard]] std::size_t index(int x, int y) const {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(size_)) +
               static_cast<std::size_t>(x);
    }
    // The k-th cell of `line` counting from the wall the tiles slide toward.
    [[nodiscard]] Pos cellAlong(Direction dir, int line, int k) const;
    // Slide one row/column into `next`; true if anything in it moved or merged.
    bool slideLine(Direction dir, int line, Cells& next, MoveResult* out, int& gained) const;
    void spawnTile(MoveResult* out);

    int size_;
    Cells cells_{};
    int score_ = 0;
    Cells undoCells_{};
    int undoScore_ = 0;
    bool canUndo_ = false;
    std::mt19937 rng_;
};

} // namespace og
