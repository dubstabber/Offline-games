#include "games/tictactoe/TicTacToeBot.hpp"

#include <array>
#include <limits>
#include <optional>
#include <vector>

namespace og {
namespace {

using Cell = TicTacToeBoard::Cell;

// The 8 winning lines (3 rows, 3 cols, 2 diagonals), as cell indices 0..8.
constexpr std::array<std::array<std::size_t, 3>, 8> kLines = {{
    {0, 1, 2},
    {3, 4, 5},
    {6, 7, 8}, // rows
    {0, 3, 6},
    {1, 4, 7},
    {2, 5, 8}, // cols
    {0, 4, 8},
    {2, 4, 6}, // diagonals
}};

Cell opponentOf(Cell player) {
    return player == Cell::X ? Cell::O : Cell::X;
}

std::vector<std::size_t> emptyCells(const TicTacToeBoard& board) {
    std::vector<std::size_t> cells;
    for (std::size_t i = 0; i < TicTacToeBoard::kSize; ++i) {
        if (board.at(i) == Cell::Empty) {
            cells.push_back(i);
        }
    }
    return cells;
}

} // namespace

TicTacToeBot::TicTacToeBot(unsigned seed) : rng_(seed) {}

std::size_t TicTacToeBot::randomMove(const TicTacToeBoard& board) {
    const std::vector<std::size_t> cells = emptyCells(board);
    std::uniform_int_distribution<std::size_t> dist(0, cells.size() - 1);
    return cells.at(dist(rng_));
}

std::optional<std::size_t> TicTacToeBot::winningMove(const TicTacToeBoard& board, Cell player) {
    // A line wins now if two of its cells are `player` and the third is empty.
    for (const auto& line : kLines) {
        int owned = 0;
        std::size_t emptyIndex = 0;
        bool hasEmpty = false;
        for (const std::size_t index : line) {
            const Cell cell = board.at(index);
            if (cell == player) {
                ++owned;
            } else if (cell == Cell::Empty) {
                emptyIndex = index;
                hasEmpty = true;
            }
        }
        if (owned == 2 && hasEmpty) {
            return emptyIndex;
        }
    }
    return std::nullopt;
}

std::size_t TicTacToeBot::mediumMove(const TicTacToeBoard& board) {
    const Cell me = board.currentPlayer();
    if (const auto win = winningMove(board, me)) {
        return *win; // take the win
    }
    if (const auto block = winningMove(board, opponentOf(me))) {
        return *block; // block the opponent's win
    }
    return randomMove(board);
}

int TicTacToeBot::minimax(TicTacToeBoard board, Cell bot, int depth) {
    if (board.isOver()) {
        if (const auto winner = board.winner()) {
            return *winner == bot ? 10 - depth : depth - 10;
        }
        return 0; // draw
    }
    const bool maximizing = board.currentPlayer() == bot;
    int best = maximizing ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    for (std::size_t i = 0; i < TicTacToeBoard::kSize; ++i) {
        if (board.at(i) != Cell::Empty) {
            continue;
        }
        TicTacToeBoard next = board;
        next.place(i);
        const int value = minimax(next, bot, depth + 1);
        best = maximizing ? std::max(best, value) : std::min(best, value);
    }
    return best;
}

std::size_t TicTacToeBot::bestMove(const TicTacToeBoard& board) {
    const Cell me = board.currentPlayer();
    std::size_t choice = 0;
    int bestValue = std::numeric_limits<int>::min();
    for (std::size_t i = 0; i < TicTacToeBoard::kSize; ++i) {
        if (board.at(i) != Cell::Empty) {
            continue;
        }
        TicTacToeBoard next = board;
        next.place(i);
        const int value = minimax(next, me, 1);
        if (value > bestValue) {
            bestValue = value;
            choice = i;
        }
    }
    return choice;
}

std::size_t TicTacToeBot::chooseMove(const TicTacToeBoard& board, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return randomMove(board);
    case Difficulty::Medium:
        return mediumMove(board);
    case Difficulty::Hard:
        return bestMove(board);
    }
    return randomMove(board);
}

} // namespace og
