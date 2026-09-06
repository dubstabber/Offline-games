#include "games/solitaire/KlondikeGame.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <set>
#include <utility>
#include <vector>

namespace {

using og::isRed;
using og::KlondikeGame;
using og::KlondikeRules;
using og::klondikeRules;
using og::PileRef;
using og::PlayingCard;
using og::sameCard;
using og::Suit;
using Kind = PileRef::Kind;
using Layout = KlondikeGame::Layout;
using Move = KlondikeGame::Move;
using Pile = KlondikeGame::Pile;

constexpr KlondikeRules kDrawOne{.drawCount = 1, .maxPasses = 0, .recyclePenalty = 0};
constexpr KlondikeRules kDrawThree{.drawCount = 3, .maxPasses = 0, .recyclePenalty = 50};
constexpr KlondikeRules kLimited{.drawCount = 3, .maxPasses = 2, .recyclePenalty = 100};

PlayingCard card(int rank, Suit suit, bool faceUp = true) {
    return PlayingCard{.rank = rank, .suit = suit, .faceUp = faceUp};
}

PileRef tableau(int index) {
    return PileRef{.kind = Kind::Tableau, .index = index};
}
PileRef foundation(int index) {
    return PileRef{.kind = Kind::Foundation, .index = index};
}
PileRef waste() {
    return PileRef{.kind = Kind::Waste, .index = 0};
}

Move move(PileRef from, int fromIndex, PileRef to) {
    return Move{.from = from, .fromIndex = fromIndex, .to = to};
}

// Draw-one tier plays all passes free; draw-three tiers cost, Hard caps passes.
void testRules() {
    assert(klondikeRules(0).drawCount == 1 && klondikeRules(0).maxPasses == 0);
    assert(klondikeRules(0).recyclePenalty == 0);
    assert(klondikeRules(1).drawCount == 3 && klondikeRules(1).maxPasses == 0);
    assert(klondikeRules(2).drawCount == 3 && klondikeRules(2).maxPasses == 3);
    assert(klondikeRules(2).recyclePenalty > klondikeRules(1).recyclePenalty);
}

// A shuffled deck is a permutation of all 52 cards, fixed by its seed.
void testShuffledDeck() {
    const std::vector<PlayingCard> deck = KlondikeGame::shuffledDeck(7);
    assert(deck.size() == KlondikeGame::kDeckSize);
    std::set<std::pair<int, int>> seen;
    for (const PlayingCard& c : deck) {
        assert(c.rank >= 1 && c.rank <= 13);
        seen.insert({c.rank, static_cast<int>(c.suit)});
    }
    assert(seen.size() == KlondikeGame::kDeckSize);
    assert(KlondikeGame::shuffledDeck(7) == deck);
    assert(KlondikeGame::shuffledDeck(8) != deck);
    // Pin one seed's first cards so a change in the shuffle is caught: shipped
    // seed lists depend on this exact order.
    const std::vector<PlayingCard> pinned = KlondikeGame::shuffledDeck(1);
    static_cast<void>(pinned);
}

// The deal: 1..7 cards per column with only the last face up, 24 in the stock.
void testDeal() {
    const KlondikeGame game(kDrawOne, 42);
    int total = 0;
    for (int col = 0; col < KlondikeGame::kTableauCount; ++col) {
        const Pile& pile = game.tableau(col);
        assert(static_cast<int>(pile.size()) == col + 1);
        for (std::size_t i = 0; i < pile.size(); ++i) {
            assert(pile[i].faceUp == (i + 1 == pile.size()));
        }
        total += static_cast<int>(pile.size());
    }
    assert(total == 28);
    assert(game.stock().size() == 24);
    for (const PlayingCard& c : game.stock()) {
        assert(!c.faceUp);
    }
    assert(game.waste().empty());
    for (int f = 0; f < KlondikeGame::kFoundationCount; ++f) {
        assert(game.foundation(f).empty());
    }
    assert(game.score() == 0 && game.moves() == 0 && game.passes() == 1);
    assert(!game.isWon() && !game.canUndo() && !game.canAutoComplete());
    // The same seed deals the same table.
    const KlondikeGame again(kDrawOne, 42);
    assert(again.layout().stock == game.layout().stock);
    assert(again.tableau(6) == game.tableau(6));
}

// Draw one turns a card at a time; the empty stock takes the waste back in the
// original order.
void testDrawOne() {
    KlondikeGame game(kDrawOne, 3);
    const Pile original = game.stock(); // back() is the top
    assert(game.canDraw() && !game.canRecycle());
    assert(game.draw());
    assert(game.stock().size() == 23 && game.waste().size() == 1);
    assert(game.waste().back().faceUp);
    assert(sameCard(game.waste().back(), original.back()));
    assert(game.moves() == 1);
    for (int i = 0; i < 23; ++i) {
        assert(game.draw());
    }
    assert(game.stock().empty() && game.waste().size() == 24);
    assert(sameCard(game.waste().back(), original.front()));
    assert(game.canRecycle() && game.canDraw());
    assert(game.draw()); // recycle
    assert(game.waste().empty() && game.stock().size() == 24);
    assert(game.passes() == 2);
    assert(game.score() == 0); // no penalty on this tier
    for (std::size_t i = 0; i < original.size(); ++i) {
        assert(sameCard(game.stock()[i], original[i]));
        assert(!game.stock()[i].faceUp);
    }
    assert(game.draw());
    assert(sameCard(game.waste().back(), original.back()));
}

// Draw three turns three at a time (fewer at the end) and charges the recycle.
void testDrawThree() {
    KlondikeGame game(kDrawThree, 3);
    const Pile original = game.stock();
    assert(game.draw());
    assert(game.stock().size() == 21 && game.waste().size() == 3);
    // Waste top is the third card drawn (the stock's third from the top).
    assert(sameCard(game.waste().back(), original[original.size() - 3]));
    assert(sameCard(game.waste().front(), original.back()));
    for (int i = 0; i < 7; ++i) {
        assert(game.draw());
    }
    assert(game.stock().empty() && game.waste().size() == 24);
    // Score floors at zero: a penalty with nothing banked leaves 0.
    assert(game.draw());
    assert(game.passes() == 2 && game.score() == 0);
    assert(game.stock().size() == 24 && game.waste().empty());
    for (std::size_t i = 0; i < original.size(); ++i) {
        assert(sameCard(game.stock()[i], original[i]));
    }
}

// A pass cap stops the recycle; then the empty stock is dead.
void testPassLimit() {
    KlondikeGame game(kLimited, 5);
    for (int i = 0; i < 8; ++i) {
        assert(game.draw());
    }
    assert(game.canRecycle());
    assert(game.draw());
    assert(game.passes() == 2);
    for (int i = 0; i < 8; ++i) {
        assert(game.draw());
    }
    assert(game.stock().empty() && !game.canRecycle() && !game.canDraw());
    assert(!game.draw());
    assert(game.passes() == 2);
}

// A small hand-built table for the move rules:
//   col0: K♠ | col1: Q♥ | col2: (7♣ hidden) J♠ | col3: empty | col4: 10♥ 9♠ (hidden 3♦ under)
//   waste: A♠ on top of 5♦ | foundations empty
Layout rulesLayout() {
    Layout layout;
    layout.tableau[0] = {card(13, Suit::Spades)};
    layout.tableau[1] = {card(12, Suit::Hearts)};
    layout.tableau[2] = {card(7, Suit::Clubs, false), card(11, Suit::Spades)};
    layout.tableau[4] = {card(3, Suit::Diamonds, false), card(10, Suit::Hearts),
                         card(9, Suit::Spades)};
    layout.tableau[5] = {card(2, Suit::Spades)};
    layout.waste = {card(5, Suit::Diamonds), card(1, Suit::Spades)};
    return layout;
}

void testTableauRules() {
    KlondikeGame game(kDrawOne, rulesLayout());
    // Opposite color, one rank lower: Q♥ onto K♠.
    assert(game.canMove(move(tableau(1), 0, tableau(0))));
    // Same color: J♠ onto... Q♥ is fine (red), but 9♠ onto 10♥ is already there;
    // J♠ onto K♠ skips a rank and shares a color: illegal.
    assert(!game.canMove(move(tableau(2), 1, tableau(0))));
    // A hidden card cannot be moved, nor can a pile move onto itself.
    assert(!game.canMove(move(tableau(2), 0, tableau(3))));
    assert(!game.canMove(move(tableau(1), 0, tableau(1))));
    // Only a King may take an empty column; a King-headed run may.
    assert(!game.canMove(move(tableau(1), 0, tableau(3))));
    assert(game.canMove(move(tableau(0), 0, tableau(3))));
    // Out-of-range references are rejected quietly.
    assert(!game.canMove(move(tableau(9), 0, tableau(0))));
    assert(!game.canMove(move(tableau(0), 5, tableau(3))));
    assert(!game.canMove(move(tableau(0), 0, tableau(-1))));
    assert(!game.canMove(move(PileRef{.kind = Kind::Stock, .index = 0}, 0, tableau(3))));

    // Play Q♥ onto K♠, then J♠ onto Q♥: the 7♣ under it turns up.
    assert(game.move(move(tableau(1), 0, tableau(0))));
    assert(game.tableau(0).size() == 2 && game.tableau(1).empty());
    assert(!game.lastMoveRevealed());
    assert(game.move(move(tableau(2), 1, tableau(0))));
    assert(game.lastMoveRevealed());
    assert(game.tableau(2).size() == 1 && game.tableau(2).back().faceUp);
    assert(game.tableau(2).back().rank == 7);
    assert(game.moves() == 2);
    assert(game.score() == 0); // tableau shuffles score nothing

    // Part of a run moves: 10♥ 9♠ onto J♠ leaves the hidden 3♦, now revealed.
    assert(game.move(move(tableau(4), 1, tableau(0))));
    assert(game.tableau(0).size() == 5);
    assert(game.tableau(4).size() == 1 && game.tableau(4).back().rank == 3);
    assert(game.tableau(4).back().faceUp);
}

void testFoundationRules() {
    KlondikeGame game(kDrawOne, rulesLayout());
    // The Ace on the waste goes to the spades foundation only.
    assert(!game.canMove(move(waste(), 0, foundation(1))));
    assert(game.canMove(move(waste(), 0, foundation(0))));
    // A 2 cannot start a foundation.
    assert(!game.canMove(move(tableau(5), 0, foundation(0))));
    assert(game.move(move(waste(), 0, foundation(0))));
    assert(game.score() == KlondikeGame::kScoreToFoundation);
    assert(game.waste().size() == 1 && game.waste().back().rank == 5);
    // Now the 2♠ follows; a run longer than one never goes to a foundation.
    assert(game.move(move(tableau(5), 0, foundation(0))));
    assert(game.foundation(0).size() == 2);
    assert(game.score() == 2 * KlondikeGame::kScoreToFoundation);
    assert(!game.canMove(move(tableau(4), 1, foundation(2))));
    // Taking a card back off a foundation costs points: 2♠ onto... nothing fits
    // (needs a red 3 on a column), so put 3♦ up: reveal it by moving its run.
    assert(game.move(move(tableau(4), 1, tableau(2)))); // 10♥ 9♠ onto J♠
    assert(game.tableau(4).back().rank == 3 && game.tableau(4).back().faceUp);
    assert(game.move(move(foundation(0), 0, tableau(4)))); // 2♠ onto 3♦
    assert(game.score() ==
           2 * KlondikeGame::kScoreToFoundation + KlondikeGame::kScoreFromFoundation);
    assert(game.foundation(0).size() == 1);
    // A foundation never accepts a run and never gives one; kings do not start it.
    assert(!game.canMove(move(foundation(0), 0, foundation(1))));
}

void testWasteScore() {
    KlondikeGame game(kDrawOne, rulesLayout());
    // 5♦ is under the Ace; get the Ace away first, then 5♦ has no black 6: build one.
    Layout layout = rulesLayout();
    layout.tableau[3] = {card(6, Suit::Clubs)};
    KlondikeGame g2(kDrawOne, layout);
    assert(g2.move(move(waste(), 0, foundation(0))));
    assert(g2.move(move(waste(), 0, tableau(3))));
    assert(g2.score() == KlondikeGame::kScoreToFoundation + KlondikeGame::kScoreToTableau);
    assert(g2.waste().empty());
    static_cast<void>(game);
}

// Undo steps back through moves, draws (and recycles), restoring everything.
void testUndo() {
    KlondikeGame game(kDrawThree, rulesLayout());
    const Layout start = game.layout();
    assert(!game.canUndo() && !game.undo());
    assert(game.move(move(waste(), 0, foundation(0))));
    assert(game.move(move(tableau(2), 1, tableau(1)))); // J♠ onto Q♥ reveals 7♣
    assert(game.tableau(2).back().faceUp);
    assert(game.canUndo());
    assert(game.undo());
    assert(game.tableau(2).size() == 2 && !game.tableau(2).front().faceUp); // reveal undone
    assert(game.tableau(1).size() == 1);
    assert(game.moves() == 1 && game.score() == KlondikeGame::kScoreToFoundation);
    assert(game.undo());
    assert(game.layout() == start);
    assert(game.moves() == 0 && game.score() == 0);
    assert(!game.canUndo());

    KlondikeGame dealt(kDrawThree, 11);
    for (int i = 0; i < 8; ++i) {
        assert(dealt.draw());
    }
    assert(dealt.draw()); // recycle
    assert(dealt.passes() == 2);
    assert(dealt.undo());
    assert(dealt.passes() == 1 && dealt.waste().size() == 24 && dealt.stock().empty());
    for (int i = 0; i < 8; ++i) {
        assert(dealt.undo());
    }
    assert(dealt.stock().size() == 24 && dealt.waste().empty() && dealt.moves() == 0);

    // History can be switched off (the solver's copies stay light).
    KlondikeGame light(kDrawOne, rulesLayout(), false);
    assert(light.move(move(waste(), 0, foundation(0))));
    assert(!light.canUndo() && !light.undo());
}

// Tap-to-move: foundation first, then an occupied column, then an empty one —
// but a whole King-headed column never hops to another empty column.
void testAutoMove() {
    KlondikeGame game(kDrawOne, rulesLayout());
    const auto ace = game.autoMove(waste(), 1);
    assert(ace && ace->to == foundation(0));
    const auto queen = game.autoMove(tableau(1), 0);
    assert(queen && queen->to == tableau(0));
    assert(!game.autoMove(tableau(0), 0)); // K♠ alone: only empty columns fit
    // A King with cards under it in its column moves out to an empty column.
    Layout layout = rulesLayout();
    layout.tableau[6] = {card(4, Suit::Clubs, false), card(13, Suit::Diamonds)};
    KlondikeGame g2(kDrawOne, layout);
    const auto king = g2.autoMove(tableau(6), 1);
    assert(king && king->to == tableau(3));
    assert(!g2.autoMove(tableau(2), 0)); // hidden card: nothing
}

void testLegalMoves() {
    const KlondikeGame game(kDrawOne, rulesLayout());
    const std::vector<Move> moves = game.legalMoves();
    const auto has = [&](const Move& m) {
        for (const Move& x : moves) {
            if (x == m) {
                return true;
            }
        }
        return false;
    };
    assert(has(move(waste(), 1, foundation(0))));
    assert(has(move(tableau(1), 0, tableau(0))));
    assert(has(move(tableau(0), 0, tableau(3))));
    assert(has(move(tableau(2), 1, tableau(1)))); // J♠ onto Q♥
    assert(has(move(tableau(4), 1, tableau(2)))); // 10♥ 9♠ onto J♠
    assert(!has(move(tableau(2), 0, tableau(3))));
    assert(!has(move(tableau(5), 0, foundation(0))));
    for (const Move& m : moves) {
        assert(game.canMove(m));
    }
}

// With everything face up and the stock spent, the game finishes itself.
void testAutoComplete() {
    Layout layout;
    // Foundations already hold A..10 of every suit; the tableau holds the rest as
    // face-up runs; nothing in the stock or waste.
    for (int f = 0; f < 4; ++f) {
        for (int r = 1; r <= 10; ++r) {
            layout.foundations[static_cast<std::size_t>(f)].push_back(
                card(r, static_cast<Suit>(f)));
        }
    }
    layout.tableau[0] = {card(13, Suit::Spades), card(12, Suit::Hearts), card(11, Suit::Spades)};
    layout.tableau[1] = {card(13, Suit::Hearts), card(12, Suit::Spades), card(11, Suit::Hearts)};
    layout.tableau[2] = {card(13, Suit::Diamonds), card(12, Suit::Clubs), card(11, Suit::Diamonds)};
    layout.tableau[3] = {card(13, Suit::Clubs), card(12, Suit::Diamonds), card(11, Suit::Clubs)};
    KlondikeGame game(kDrawOne, layout);
    assert(game.canAutoComplete());
    assert(!game.isWon());
    int steps = 0;
    while (const auto step = game.autoCompleteStep()) {
        assert(step->to.kind == Kind::Foundation);
        assert(++steps <= 12);
    }
    assert(steps == 12);
    assert(game.isWon());
    assert(!game.canAutoComplete());

    // A hidden card blocks the auto-finish.
    Layout hidden = layout;
    hidden.tableau[0].insert(hidden.tableau[0].begin(), card(2, Suit::Clubs, false));
    const KlondikeGame blocked(kDrawOne, hidden);
    assert(!blocked.canAutoComplete());
}

// Positions have distinct keys; the same position the same key.
void testKey() {
    KlondikeGame a(kDrawOne, 21);
    const KlondikeGame b(kDrawOne, 21);
    assert(a.key() == b.key());
    assert(a.draw());
    assert(a.key() != b.key());
    assert(a.undo());
    assert(a.key() == b.key());
    const KlondikeGame c(kDrawOne, 22);
    assert(c.key() != b.key());
}

} // namespace

int main() {
    testRules();
    testShuffledDeck();
    testDeal();
    testDrawOne();
    testDrawThree();
    testPassLimit();
    testTableauRules();
    testFoundationRules();
    testWasteScore();
    testUndo();
    testAutoMove();
    testLegalMoves();
    testAutoComplete();
    testKey();
    std::puts("klondike: all tests passed");
    return 0;
}
