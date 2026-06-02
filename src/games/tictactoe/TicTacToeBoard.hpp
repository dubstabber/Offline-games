#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace og {

// Pure game logic for Tic-Tac-Toe: no SDL, no rendering, fully unit-testable.
// The rendering layer (TicTacToeScene) reads this and draws it. Cells are
// indexed 0..8, row-major.
class TicTacToeBoard {
public:
    enum class Cell : std::uint8_t { Empty, X, O };

    static constexpr std::size_t kSize = 9;

    [[nodiscard]] Cell at(std::size_t index) const { return cells_.at(index); }
    [[nodiscard]] Cell currentPlayer() const { return current_; }

    // Place the current player's mark. Returns false (no-op) if the index is
    // out of range, the cell is occupied, or the game is already over.
    bool place(std::size_t index);

    // The winning player, if any. nullopt while the game is ongoing or drawn.
    [[nodiscard]] std::optional<Cell> winner() const { return winner_; }
    [[nodiscard]] bool isDraw() const;
    [[nodiscard]] bool isOver() const { return winner_.has_value() || isFull(); }

    void reset();

private:
    [[nodiscard]] bool isFull() const;
    void recomputeWinner();

    std::array<Cell, kSize> cells_{};
    Cell current_ = Cell::X;
    std::optional<Cell> winner_;
};

} // namespace og
