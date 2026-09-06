#include "games/memory/MemoryBoard.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <vector>

namespace {

using og::MemoryBoard;
using og::MemoryLayout;
using og::memoryLayout;
using Outcome = MemoryBoard::Outcome;
using Player = MemoryBoard::Player;

// Index of the other card carrying the same picture as `index`.
int partnerOf(const MemoryBoard& board, int index) {
    for (int i = 0; i < board.cardCount(); ++i) {
        if (i != index && board.card(i).picture == board.card(index).picture) {
            return i;
        }
    }
    assert(false && "every picture is dealt twice");
    return -1;
}

// Any card still on the table whose picture differs from card `index`.
int mismatchFor(const MemoryBoard& board, int index) {
    for (int i = 0; i < board.cardCount(); ++i) {
        if (!board.card(i).matched && board.card(i).picture != board.card(index).picture) {
            return i;
        }
    }
    assert(false && "expected a second picture on the table");
    return -1;
}

int firstOnTable(const MemoryBoard& board) {
    for (int i = 0; i < board.cardCount(); ++i) {
        if (!board.card(i).matched) {
            return i;
        }
    }
    return -1;
}

// Every difficulty's rows hold exactly its card count, and the pair counts are
// the original's 7 / 11 / 15.
void testLayouts() {
    const std::array<int, 3> expectedPairs{7, 11, 15};
    for (int d = 0; d < 3; ++d) {
        const MemoryLayout layout = memoryLayout(d);
        assert(layout.pairs == expectedPairs.at(static_cast<std::size_t>(d)));
        const int cards = std::accumulate(layout.rowCounts.begin(), layout.rowCounts.end(), 0);
        assert(cards == 2 * layout.pairs);
        for (const int count : layout.rowCounts) {
            assert(count >= 1 && count <= 5);
        }
    }
    assert(memoryLayout(0).rowCounts.size() == 4);
    assert(memoryLayout(1).rowCounts.size() == 6);
    assert(memoryLayout(2).rowCounts.size() == 6);
    assert(memoryLayout(2).rowCounts.front() == 5);
}

// A fresh deal is all face down, every picture appears on exactly two cards, and
// no two pairs share a picture.
void testDeal() {
    for (const int pairs : {7, 11, 15}) {
        const MemoryBoard board(pairs, 42);
        assert(board.pairs() == pairs);
        assert(board.cardCount() == 2 * pairs);
        assert(board.turn() == Player::You);
        assert(board.score(Player::You) == 0 && board.score(Player::Bot) == 0);
        assert(board.remainingPairs() == pairs);
        assert(!board.isOver());
        assert(!board.winner());
        assert(!board.firstFlipped() && !board.secondFlipped());
        std::array<int, MemoryBoard::kPictureCount> seen{};
        for (int i = 0; i < board.cardCount(); ++i) {
            const MemoryBoard::Card& c = board.card(i);
            assert(!c.faceUp && !c.matched);
            assert(c.picture >= 0 && c.picture < MemoryBoard::kPictureCount);
            seen.at(static_cast<std::size_t>(c.picture)) += 1;
        }
        int distinct = 0;
        for (const int n : seen) {
            assert(n == 0 || n == 2);
            distinct += n == 2 ? 1 : 0;
        }
        assert(distinct == pairs);
    }
}

// Pair counts outside the picture set are clamped.
void testPairClamp() {
    assert(MemoryBoard(0, 1).pairs() == 1);
    assert(MemoryBoard(99, 1).pairs() == MemoryBoard::kMaxPairs);
}

// The same seed deals the same table; different seeds (almost surely) differ.
void testDeterminism() {
    const MemoryBoard a(11, 7);
    const MemoryBoard b(11, 7);
    bool anyDifferent = false;
    for (int i = 0; i < a.cardCount(); ++i) {
        assert(a.card(i).picture == b.card(i).picture);
    }
    for (std::uint32_t seed = 8; seed < 16 && !anyDifferent; ++seed) {
        const MemoryBoard c(11, seed);
        for (int i = 0; i < a.cardCount(); ++i) {
            anyDifferent = anyDifferent || c.card(i).picture != a.card(i).picture;
        }
    }
    assert(anyDifferent);
}

// A card cannot be flipped twice, a third card cannot join two face-up ones, and
// resolve() with fewer than two cards up does nothing.
void testFlipRules() {
    MemoryBoard board(7, 3);
    assert(board.resolve() == Outcome::None);
    assert(board.flip(-1) == false);
    assert(board.flip(board.cardCount()) == false);

    assert(board.canFlip(0));
    assert(board.flip(0));
    assert(board.card(0).faceUp);
    assert(board.firstFlipped() == 0);
    assert(!board.secondFlipped());
    assert(!board.awaitingResolve());
    assert(!board.canFlip(0));
    assert(!board.flip(0)); // already up

    assert(board.resolve() == Outcome::None); // one card up: nothing to settle
    assert(board.card(0).faceUp);
    assert(board.firstFlipped() == 0);

    assert(board.flip(1));
    assert(board.secondFlipped() == 1);
    assert(board.awaitingResolve());
    assert(!board.canFlip(2));
    assert(!board.flip(2)); // two already up
    assert(board.turn() == Player::You);
}

// Matching cards leave the table, score for the flipper and keep the turn.
void testMatch() {
    MemoryBoard board(7, 5);
    const int a = 0;
    const int b = partnerOf(board, a);
    assert(board.flip(a) && board.flip(b));
    assert(board.resolve() == Outcome::Match);
    assert(board.card(a).matched && board.card(b).matched);
    assert(!board.card(a).faceUp && !board.card(b).faceUp);
    assert(board.score(Player::You) == 1);
    assert(board.score(Player::Bot) == 0);
    assert(board.remainingPairs() == 6);
    assert(board.turn() == Player::You);
    assert(!board.firstFlipped() && !board.secondFlipped());
    assert(!board.canFlip(a) && !board.flip(a)); // collected cards are gone
}

// Mismatched cards turn back over and the turn passes, back and forth.
void testMismatch() {
    MemoryBoard board(7, 5);
    const int a = 0;
    const int b = mismatchFor(board, a);
    assert(board.flip(a) && board.flip(b));
    assert(board.resolve() == Outcome::Mismatch);
    assert(!board.card(a).faceUp && !board.card(b).faceUp);
    assert(!board.card(a).matched && !board.card(b).matched);
    assert(board.score(Player::You) == 0 && board.score(Player::Bot) == 0);
    assert(board.turn() == Player::Bot);
    assert(board.canFlip(a));

    // The bot's mismatch hands the turn back; its match scores for the bot.
    assert(board.flip(a) && board.flip(b));
    assert(board.resolve() == Outcome::Mismatch);
    assert(board.turn() == Player::You);
    assert(board.flip(a) && board.flip(b));
    assert(board.resolve() == Outcome::Mismatch);
    assert(board.turn() == Player::Bot);
    assert(board.flip(a) && board.flip(partnerOf(board, a)));
    assert(board.resolve() == Outcome::Match);
    assert(board.score(Player::Bot) == 1);
    assert(board.turn() == Player::Bot);
}

// Collecting every pair ends the game and names the player with more pairs.
void testGameOver() {
    MemoryBoard board(7, 9);
    // The player takes the first four pairs, then misses so the bot gets the rest.
    for (int i = 0; i < 4; ++i) {
        const int a = firstOnTable(board);
        assert(board.flip(a) && board.flip(partnerOf(board, a)));
        assert(board.resolve() == Outcome::Match);
        assert(!board.isOver());
    }
    {
        const int a = firstOnTable(board);
        assert(board.flip(a) && board.flip(mismatchFor(board, a)));
        assert(board.resolve() == Outcome::Mismatch);
    }
    assert(board.turn() == Player::Bot);
    while (!board.isOver()) {
        const int a = firstOnTable(board);
        assert(board.flip(a) && board.flip(partnerOf(board, a)));
        assert(board.resolve() == Outcome::Match);
    }
    assert(board.remainingPairs() == 0);
    assert(board.score(Player::You) == 4 && board.score(Player::Bot) == 3);
    assert(board.winner() == Player::You);
    assert(board.flip(0) == false); // nothing left to flip
}

// With an even pair count a split ends in a draw (no winner). Four pairs: the
// player takes two, misses, and the bot takes the other two.
void testDraw() {
    MemoryBoard board(4, 11);
    const auto takePair = [&board] {
        const int a = firstOnTable(board);
        assert(board.flip(a) && board.flip(partnerOf(board, a)));
        assert(board.resolve() == Outcome::Match);
    };
    const auto miss = [&board] {
        const int a = firstOnTable(board);
        assert(board.flip(a) && board.flip(mismatchFor(board, a)));
        assert(board.resolve() == Outcome::Mismatch);
    };
    takePair();
    takePair();
    miss(); // You 2, bot to move with two pairs left
    assert(board.turn() == Player::Bot);
    takePair();
    takePair(); // Bot 2: the table is empty
    assert(board.isOver());
    assert(board.score(Player::You) == 2 && board.score(Player::Bot) == 2);
    assert(!board.winner());
}

// reset() deals a new table and clears the scores and the turn.
void testReset() {
    MemoryBoard board(7, 13);
    const int a = 0;
    assert(board.flip(a) && board.flip(partnerOf(board, a)));
    assert(board.resolve() == Outcome::Match);
    const int b = firstOnTable(board);
    assert(board.flip(b) && board.flip(mismatchFor(board, b)));
    assert(board.resolve() == Outcome::Mismatch);
    assert(board.turn() == Player::Bot);
    assert(board.flip(b));

    board.reset(14);
    assert(board.turn() == Player::You);
    assert(board.score(Player::You) == 0 && board.score(Player::Bot) == 0);
    assert(board.remainingPairs() == 7);
    assert(!board.firstFlipped() && !board.awaitingResolve());
    for (int i = 0; i < board.cardCount(); ++i) {
        assert(!board.card(i).faceUp && !board.card(i).matched);
    }
}

} // namespace

int main() {
    testLayouts();
    testDeal();
    testPairClamp();
    testDeterminism();
    testFlipRules();
    testMatch();
    testMismatch();
    testGameOver();
    testDraw();
    testReset();
    std::puts("memory: all tests passed");
    return 0;
}
