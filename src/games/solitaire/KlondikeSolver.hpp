#pragma once

#include "games/solitaire/KlondikeGame.hpp"

#include <vector>

namespace og {

// A "thoughtful" Klondike solver: it sees every card, face-down ones included,
// and searches for any sequence of moves and draws that wins the deal. Depth-
// first with a visited set and a node budget, trying foundation moves and
// reveals first, so winnable deals usually fall in a few thousand positions
// while hopeless ones give up at the budget. It is used offline to curate the
// shipped seed lists (see SolitaireDeals) and in tests, never at play time.
class KlondikeSolver {
public:
    // One step of a solution: a draw (stock tap) or a move.
    struct Action {
        bool draw = false;
        KlondikeGame::Move move;
    };

    struct Result {
        bool solved = false;
        // The search hit its budget: the deal is unknown, not proven unwinnable.
        bool budgetExhausted = false;
        int nodes = 0; // positions visited
        // The winning line up to the point where the table auto-completes.
        std::vector<Action> actions;
    };

    [[nodiscard]] static Result solve(const KlondikeGame& start, int nodeBudget);
};

} // namespace og
