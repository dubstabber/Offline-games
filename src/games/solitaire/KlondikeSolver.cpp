#include "games/solitaire/KlondikeSolver.hpp"

#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>

namespace og {
namespace {

using Action = KlondikeSolver::Action;
using Kind = PileRef::Kind;
using Move = KlondikeGame::Move;

// Would this tableau move turn over the card beneath the run?
[[nodiscard]] bool reveals(const KlondikeGame& game, const Move& m) {
    if (m.from.kind != Kind::Tableau || m.fromIndex == 0) {
        return false;
    }
    return !game.tableau(m.from.index).at(static_cast<std::size_t>(m.fromIndex) - 1).faceUp;
}

// A whole King-headed column shuffling to another empty column changes nothing.
[[nodiscard]] bool pointless(const KlondikeGame& game, const Move& m) {
    return m.from.kind == Kind::Tableau && m.fromIndex == 0 && m.to.kind == Kind::Tableau &&
           game.tableau(m.to.index).empty();
}

[[nodiscard]] int foundationRank(const KlondikeGame& game, Suit suit) {
    return static_cast<int>(game.foundation(static_cast<int>(suit)).size());
}

// A card can always go up once no card of the other color could still need it
// as a host: both opposite-color foundations already hold its rank minus one.
[[nodiscard]] bool safeFoundationMove(const KlondikeGame& game, const Move& m) {
    if (m.to.kind != Kind::Foundation) {
        return false;
    }
    const PlayingCard& card = game.pile(m.from).at(static_cast<std::size_t>(
        m.from.kind == Kind::Tableau ? m.fromIndex
                                     : static_cast<int>(game.pile(m.from).size()) - 1));
    if (card.rank <= 2) {
        return true;
    }
    const int needed = card.rank - 1;
    if (isRed(card.suit)) {
        return foundationRank(game, Suit::Spades) >= needed &&
               foundationRank(game, Suit::Clubs) >= needed;
    }
    return foundationRank(game, Suit::Hearts) >= needed &&
           foundationRank(game, Suit::Diamonds) >= needed;
}

// Splitting a run only pays when the card it uncovers can go straight up.
[[nodiscard]] bool usefulSplit(const KlondikeGame& game, const Move& m) {
    if (m.from.kind != Kind::Tableau || m.fromIndex == 0) {
        return true;
    }
    const PlayingCard& below =
        game.tableau(m.from.index).at(static_cast<std::size_t>(m.fromIndex) - 1);
    if (!below.faceUp) {
        return true; // a reveal, not a split
    }
    return below.rank == foundationRank(game, below.suit) + 1;
}

// The child actions of a position, most promising first: cards to the
// foundations, moves that reveal a card, plays off the waste, the rest of the
// tableau shuffles, and finally a stock tap. A "safe" foundation move is forced
// on its own, and foundation-to-tableau moves are left out: both enlarge the
// search far more than they win.
[[nodiscard]] std::vector<Action> orderedActions(const KlondikeGame& game) {
    std::vector<Action> out;
    const std::vector<Move> moves = game.legalMoves();
    for (const Move& m : moves) {
        if (safeFoundationMove(game, m)) {
            out.push_back(Action{.draw = false, .move = m});
            return out;
        }
    }
    const auto take = [&](auto&& pred) {
        for (const Move& m : moves) {
            if (m.from.kind != Kind::Foundation && !pointless(game, m) && usefulSplit(game, m) &&
                pred(m)) {
                out.push_back(Action{.draw = false, .move = m});
            }
        }
    };
    take([](const Move& m) { return m.to.kind == Kind::Foundation; });
    take([&](const Move& m) { return m.to.kind == Kind::Tableau && reveals(game, m); });
    take([&](const Move& m) { return m.to.kind == Kind::Tableau && m.from.kind == Kind::Waste; });
    take([&](const Move& m) {
        return m.to.kind == Kind::Tableau && m.from.kind == Kind::Tableau && !reveals(game, m);
    });
    if (game.canDraw()) {
        out.push_back(Action{.draw = true, .move = {}});
    }
    return out;
}

struct Frame {
    KlondikeGame game;
    std::vector<Action> actions;
    std::size_t next = 0;
    Action via; // the action that produced this position (unused for the root)
};

} // namespace

KlondikeSolver::Result KlondikeSolver::solve(const KlondikeGame& start, int nodeBudget) {
    Result result;
    KlondikeGame root = start.searchCopy();
    std::unordered_set<std::string> visited;
    visited.insert(root.key());
    std::vector<Frame> stack;
    stack.push_back(Frame{.game = std::move(root), .actions = {}, .next = 0, .via = {}});
    stack.back().actions = orderedActions(stack.back().game);

    while (!stack.empty()) {
        const KlondikeGame& game = stack.back().game;
        if (game.isWon() || game.canAutoComplete()) {
            result.solved = true;
            result.actions.reserve(stack.size());
            for (std::size_t i = 1; i < stack.size(); ++i) {
                result.actions.push_back(stack.at(i).via);
            }
            return result;
        }
        Frame& frame = stack.back();
        if (frame.next >= frame.actions.size()) {
            stack.pop_back();
            continue;
        }
        const Action action = frame.actions.at(frame.next++);
        KlondikeGame child = frame.game;
        if (action.draw) {
            child.draw();
        } else {
            child.move(action.move);
        }
        if (!visited.insert(child.key()).second) {
            continue;
        }
        if (++result.nodes > nodeBudget) {
            result.budgetExhausted = true;
            return result;
        }
        stack.push_back(Frame{.game = std::move(child), .actions = {}, .next = 0, .via = action});
        stack.back().actions = orderedActions(stack.back().game);
    }
    return result;
}

} // namespace og
