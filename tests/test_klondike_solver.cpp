#include "games/solitaire/KlondikeGame.hpp"
#include "games/solitaire/KlondikeSolver.hpp"

#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

using og::KlondikeGame;
using og::KlondikeRules;
using og::klondikeRules;
using og::KlondikeSolver;
using og::PlayingCard;
using og::Suit;
using Layout = KlondikeGame::Layout;
using Result = KlondikeSolver::Result;

constexpr KlondikeRules kDrawOne{.drawCount = 1, .maxPasses = 0, .recyclePenalty = 0};

PlayingCard card(int rank, Suit suit, bool faceUp = true) {
    return PlayingCard{.rank = rank, .suit = suit, .faceUp = faceUp};
}

// Replay a solution on a fresh copy of the deal (with history, like real play):
// every action must be legal and the table must then finish itself.
void verifySolution(const KlondikeGame& deal, const Result& result) {
    KlondikeGame game(deal.rules(), deal.layout());
    for (const KlondikeSolver::Action& a : result.actions) {
        assert(a.draw ? game.draw() : game.move(a.move));
    }
    assert(game.isWon() || game.canAutoComplete());
    int steps = 0;
    while (!game.isWon()) {
        assert(game.autoCompleteStep());
        assert(++steps <= KlondikeGame::kDeckSize);
    }
}

// A finished table is solved on the spot.
void testWonPosition() {
    Layout layout;
    for (int f = 0; f < 4; ++f) {
        for (int r = 1; r <= 13; ++r) {
            layout.foundations[static_cast<std::size_t>(f)].push_back(
                card(r, static_cast<Suit>(f)));
        }
    }
    const KlondikeGame game(kDrawOne, layout);
    assert(game.isWon());
    const Result result = KlondikeSolver::solve(game, 100);
    assert(result.solved && result.actions.empty() && result.nodes == 0);
}

// A table with no legal move and nothing to draw is proven unwinnable.
void testDeadTable() {
    Layout layout;
    layout.tableau[0] = {card(1, Suit::Spades, false), card(13, Suit::Hearts)};
    layout.tableau[1] = {card(1, Suit::Hearts, false), card(13, Suit::Spades)};
    layout.tableau[2] = {card(1, Suit::Diamonds, false), card(13, Suit::Clubs)};
    layout.tableau[3] = {card(1, Suit::Clubs, false), card(13, Suit::Diamonds)};
    layout.tableau[4] = {card(5, Suit::Diamonds, false), card(2, Suit::Clubs)};
    layout.tableau[5] = {card(6, Suit::Diamonds, false), card(2, Suit::Diamonds)};
    layout.tableau[6] = {card(7, Suit::Clubs, false), card(2, Suit::Hearts)};
    const KlondikeGame game(kDrawOne, layout);
    assert(game.legalMoves().empty() && !game.canDraw());
    const Result result = KlondikeSolver::solve(game, 1000);
    assert(!result.solved && !result.budgetExhausted);
}

// A tiny budget on a real deal reports "unknown", not "unwinnable".
void testBudget() {
    const KlondikeGame game(kDrawOne, 1);
    const Result result = KlondikeSolver::solve(game, 1);
    assert(!result.solved && result.budgetExhausted);
}

// Real draw-one deals: most are won within a modest budget, and every solution
// replays legally to a win.
void testSolvesDeals() {
    int solved = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::uint32_t seed = 1; seed <= 10; ++seed) {
        const KlondikeGame game(klondikeRules(0), seed);
        const Result result = KlondikeSolver::solve(game, 8000);
        if (result.solved) {
            ++solved;
            assert(!result.actions.empty());
            verifySolution(game, result);
        }
    }
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0)
            .count();
    std::printf("draw-one: %d/10 solved in %lld ms\n", solved, static_cast<long long>(ms));
    assert(solved >= 4);
}

// The pass-capped draw-three tier is harder but still yields winnable deals,
// and those solutions never need a forbidden recycle.
void testSolvesLimitedDeals() {
    int solved = 0;
    for (std::uint32_t seed = 1; seed <= 20; ++seed) {
        const KlondikeGame game(klondikeRules(2), seed);
        const Result result = KlondikeSolver::solve(game, 8000);
        if (result.solved) {
            ++solved;
            verifySolution(game, result);
        }
    }
    std::printf("draw-three, 3 passes: %d/20 solved\n", solved);
    assert(solved >= 1);
}

} // namespace

int main() {
    testWonPosition();
    testDeadTable();
    testBudget();
    testSolvesDeals();
    testSolvesLimitedDeals();
    std::puts("klondike_solver: all tests passed");
    return 0;
}
