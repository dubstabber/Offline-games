#pragma once

#include <cstdint>
#include <vector>

namespace og {

// Pure Minesweeper game logic: no SDL, no rendering, fully unit-testable. The
// rendering layer (MineSweeperScene) reads this and draws it. Cells are addressed
// by (row, column); mines are placed lazily on the first reveal so the first tap
// is always safe and the board is guaranteed solvable by logic (see
// MineSweeperSolver).
class MineSweeperBoard {
public:
    enum class State : std::uint8_t { Playing, Won, Lost };

    struct Cell {
        bool mine = false;
        bool revealed = false;
        bool flagged = false;
        bool exploded = false;     // the specific mine the player detonated
        std::uint8_t adjacent = 0; // adjacent mine count (valid once started)
    };

    // A blank, all-covered board of the given shape. `seed` seeds the layout
    // generated on the first reveal.
    MineSweeperBoard(int width, int height, int mineCount, std::uint32_t seed);

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] int mineCount() const { return mineCount_; }
    [[nodiscard]] int flagCount() const { return flagCount_; }
    // Mines minus flags placed — what the original shows in its 💣 counter.
    [[nodiscard]] int minesRemaining() const { return mineCount_ - flagCount_; }
    [[nodiscard]] State state() const { return state_; }
    [[nodiscard]] bool started() const { return started_; } // mines placed yet?
    [[nodiscard]] bool inBounds(int r, int c) const;
    [[nodiscard]] const Cell& at(int r, int c) const; // caller ensures in-bounds

    // Reveal a cell. The first reveal generates a solvable layout safe at (r,c) and
    // its neighbours; reveals of a zero-cell flood through the connected region.
    // No-op if the game is over or the cell is flagged/already revealed.
    void reveal(int r, int c);
    // Toggle a flag on a covered cell (no-op once revealed or after game over).
    void toggleFlag(int r, int c);
    // Chord: on a revealed number whose adjacent flags equal its count, reveal the
    // other neighbours. A wrong flag detonates a real mine, exactly as the original.
    void chord(int r, int c);

    // Place a known mine layout (row-major, size width*height): computes adjacency
    // and marks the board started. For tests and internal generation.
    void setLayout(const std::vector<std::uint8_t>& layout);

private:
    [[nodiscard]] int index(int r, int c) const { return (r * width_) + c; }
    [[nodiscard]] Cell& cell(int r, int c);

    template <class F> void forEachNeighbor(int r, int c, const F& fn) const {
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0) {
                    continue;
                }
                const int nr = r + dr;
                const int nc = c + dc;
                if (inBounds(nr, nc)) {
                    fn(nr, nc);
                }
            }
        }
    }

    void computeAdjacency();
    void floodReveal(int r, int c); // reveal (r,c) and cascade through zero-cells
    void revealAllMines();          // on loss, uncover every mine
    void checkWin();
    [[nodiscard]] int adjacentFlags(int r, int c) const;

    int width_;
    int height_;
    int mineCount_;
    std::uint32_t seed_;
    std::vector<Cell> cells_;
    State state_ = State::Playing;
    bool started_ = false;
    int flagCount_ = 0;
    int revealedSafe_ = 0; // revealed non-mine cells, for the win check
};

} // namespace og
