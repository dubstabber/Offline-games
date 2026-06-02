#include "games/Difficulty.hpp"
#include "games/tictactoe/TicTacToeBoard.hpp"
#include "games/tictactoe/TicTacToeBot.hpp"

#include <cassert>
#include <cstdio>

namespace {

using og::Difficulty;
using og::TicTacToeBoard;
using og::TicTacToeBot;
using Cell = og::TicTacToeBoard::Cell;

// Easy always returns a legal (in-range, empty) cell.
void testEasyReturnsLegalMove() {
    TicTacToeBot bot(123);
    for (int trial = 0; trial < 50; ++trial) {
        TicTacToeBoard board;
        const std::size_t move = bot.chooseMove(board, Difficulty::Easy);
        assert(move < TicTacToeBoard::kSize);
        assert(board.at(move) == Cell::Empty);
    }
}

// With O to move and O at {0,1}, both Medium and Hard take the win at 2.
void testTakesImmediateWin() {
    TicTacToeBoard board;
    for (std::size_t i : {4U, 0U, 3U, 1U, 6U}) { // X4 O0 X3 O1 X6 -> O to move
        board.place(i);
    }
    assert(board.currentPlayer() == Cell::O);
    TicTacToeBot bot(1);
    assert(bot.chooseMove(board, Difficulty::Medium) == 2);
    assert(bot.chooseMove(board, Difficulty::Hard) == 2);
}

// X threatens {0,1}; both Medium and Hard block at 2.
void testBlocksImmediateLoss() {
    TicTacToeBoard board;
    for (std::size_t i : {0U, 4U, 1U}) { // X0 O4 X1 -> O to move, X threatens 2
        board.place(i);
    }
    assert(board.currentPlayer() == Cell::O);
    TicTacToeBot bot(1);
    assert(bot.chooseMove(board, Difficulty::Medium) == 2);
    assert(bot.chooseMove(board, Difficulty::Hard) == 2);
}

// Exhaustively: against an optimal (Hard) O, no line of X play can win.
// X explores every move; O always answers with Hard. O must never lose.
void playOutHard(TicTacToeBoard board, TicTacToeBot& bot) {
    if (board.isOver()) {
        assert(board.winner() != Cell::X); // the bot (O) never loses
        return;
    }
    if (board.currentPlayer() == Cell::X) {
        for (std::size_t i = 0; i < TicTacToeBoard::kSize; ++i) {
            if (board.at(i) == Cell::Empty) {
                TicTacToeBoard next = board;
                next.place(i);
                playOutHard(next, bot);
            }
        }
    } else {
        board.place(bot.chooseMove(board, Difficulty::Hard));
        playOutHard(board, bot);
    }
}

void testHardNeverLoses() {
    TicTacToeBot bot(7);
    playOutHard(TicTacToeBoard{}, bot);
}

// Hard vs Hard from the empty board is a draw (perfect play).
void testHardVsHardIsDraw() {
    TicTacToeBoard board;
    TicTacToeBot bot(7);
    while (!board.isOver()) {
        board.place(bot.chooseMove(board, Difficulty::Hard));
    }
    assert(board.isDraw());
    assert(!board.winner().has_value());
}

} // namespace

int main() {
    testEasyReturnsLegalMove();
    testTakesImmediateWin();
    testBlocksImmediateLoss();
    testHardNeverLoses();
    testHardVsHardIsDraw();
    std::puts("All TicTacToeBot tests passed.");
    return 0;
}
