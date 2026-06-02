#include "games/tictactoe/TicTacToeBoard.hpp"

#include <algorithm>

namespace og {
namespace {

// The eight winning lines (rows, columns, diagonals).
constexpr std::array<std::array<std::size_t, 3>, 8> kLines{{
    {0, 1, 2},
    {3, 4, 5},
    {6, 7, 8}, // rows
    {0, 3, 6},
    {1, 4, 7},
    {2, 5, 8}, // columns
    {0, 4, 8},
    {2, 4, 6}, // diagonals
}};

} // namespace

bool TicTacToeBoard::place(std::size_t index) {
    if (index >= kSize || cells_.at(index) != Cell::Empty || isOver()) {
        return false;
    }
    cells_.at(index) = current_;
    recomputeWinner();
    if (!winner_.has_value()) {
        current_ = current_ == Cell::X ? Cell::O : Cell::X;
    }
    return true;
}

void TicTacToeBoard::recomputeWinner() {
    for (const auto& line : kLines) {
        const Cell first = cells_.at(line[0]);
        if (first != Cell::Empty && cells_.at(line[1]) == first && cells_.at(line[2]) == first) {
            winner_ = first;
            return;
        }
    }
}

bool TicTacToeBoard::isFull() const {
    return std::ranges::none_of(cells_, [](Cell cell) { return cell == Cell::Empty; });
}

bool TicTacToeBoard::isDraw() const {
    return !winner_.has_value() && isFull();
}

void TicTacToeBoard::reset() {
    cells_.fill(Cell::Empty);
    current_ = Cell::X;
    winner_.reset();
}

} // namespace og
