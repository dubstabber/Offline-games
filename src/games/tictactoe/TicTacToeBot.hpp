#pragma once

#include "games/Difficulty.hpp"
#include "games/tictactoe/TicTacToeBoard.hpp"

#include <cstddef>
#include <optional>
#include <random>

namespace og {

// Pure (SDL-free) opponent for Tic-Tac-Toe. Given a board where it is the
// current player's turn, it picks a cell index to play. Behaviour scales with
// difficulty; the RNG is seedable so the random tiers are unit-testable.
class TicTacToeBot {
public:
    explicit TicTacToeBot(unsigned seed);

    // Choose an empty cell for the current player. Assumes the board is not
    // over and has at least one empty cell.
    [[nodiscard]] std::size_t chooseMove(const TicTacToeBoard& board, Difficulty difficulty);

private:
    std::size_t randomMove(const TicTacToeBoard& board);
    // Immediate win for `player` this turn, else nullopt.
    static std::optional<std::size_t> winningMove(const TicTacToeBoard& board,
                                                  TicTacToeBoard::Cell player);
    std::size_t mediumMove(const TicTacToeBoard& board);
    // Perfect play (minimax). Returns the best cell for the current player.
    static std::size_t bestMove(const TicTacToeBoard& board);
    static int minimax(TicTacToeBoard board, TicTacToeBoard::Cell bot, int depth);

    std::mt19937 rng_;
};

} // namespace og
