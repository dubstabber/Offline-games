#include "games/tictactoe/TicTacToeBoard.hpp"

#include <cassert>
#include <cstdio>

namespace {

using og::TicTacToeBoard;
using Cell = og::TicTacToeBoard::Cell;

void testStartsEmptyWithX() {
    TicTacToeBoard board;
    assert(board.currentPlayer() == Cell::X);
    assert(!board.isOver());
    assert(!board.winner().has_value());
    for (std::size_t i = 0; i < TicTacToeBoard::kSize; ++i) {
        assert(board.at(i) == Cell::Empty);
    }
}

void testTurnsAlternate() {
    TicTacToeBoard board;
    assert(board.place(0));
    assert(board.at(0) == Cell::X);
    assert(board.currentPlayer() == Cell::O);
    assert(board.place(1));
    assert(board.at(1) == Cell::O);
    assert(board.currentPlayer() == Cell::X);
}

void testRejectsOccupiedAndOutOfRange() {
    TicTacToeBoard board;
    assert(board.place(4));
    assert(!board.place(4));                  // occupied
    assert(!board.place(99));                 // out of range
    assert(board.currentPlayer() == Cell::O); // unchanged by rejected moves
}

void testRowWin() {
    TicTacToeBoard board;
    board.place(0); // X
    board.place(3); // O
    board.place(1); // X
    board.place(4); // O
    board.place(2); // X completes top row
    assert(board.winner().has_value());
    assert(*board.winner() == Cell::X);
    assert(board.isOver());
    assert(!board.isDraw());
    assert(!board.place(5)); // no moves after a win
}

void testDiagonalWin() {
    TicTacToeBoard board;
    board.place(0); // X
    board.place(1); // O
    board.place(4); // X
    board.place(2); // O
    board.place(8); // X completes diagonal
    assert(*board.winner() == Cell::X);
}

void testDraw() {
    TicTacToeBoard board;
    // X O X / X O O / O X X  -> full board, no winner
    for (std::size_t i : {0U, 1U, 2U, 4U, 3U, 5U, 7U, 6U, 8U}) {
        board.place(i);
    }
    assert(board.isOver());
    assert(board.isDraw());
    assert(!board.winner().has_value());
}

void testReset() {
    TicTacToeBoard board;
    board.place(0);
    board.place(1);
    board.reset();
    assert(board.currentPlayer() == Cell::X);
    assert(!board.isOver());
    assert(board.at(0) == Cell::Empty);
}

} // namespace

int main() {
    testStartsEmptyWithX();
    testTurnsAlternate();
    testRejectsOccupiedAndOutOfRange();
    testRowWin();
    testDiagonalWin();
    testDraw();
    testReset();
    std::puts("All TicTacToeBoard tests passed.");
    return 0;
}
