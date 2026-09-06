#include "games/solitaire/KlondikeGame.hpp"

#include <algorithm>
#include <random>
#include <utility>

namespace og {
namespace {

using Kind = PileRef::Kind;

constexpr int kKing = kRankCount;

[[nodiscard]] char cardId(const PlayingCard& card) {
    return static_cast<char>('A' + (static_cast<int>(card.suit) * kRankCount) + (card.rank - 1));
}

// The pile a PileRef names, for a const or mutable Layout alike.
template <class LayoutT> [[nodiscard]] auto& pileIn(LayoutT& layout, PileRef ref) {
    switch (ref.kind) {
    case Kind::Stock:
        return layout.stock;
    case Kind::Waste:
        return layout.waste;
    case Kind::Foundation:
        return layout.foundations.at(static_cast<std::size_t>(ref.index));
    case Kind::Tableau:
        break;
    }
    return layout.tableau.at(static_cast<std::size_t>(ref.index));
}

} // namespace

// ---- Construction ------------------------------------------------------------

std::vector<PlayingCard> KlondikeGame::shuffledDeck(std::uint32_t seed) {
    std::vector<PlayingCard> deck;
    deck.reserve(kDeckSize);
    for (int suit = 0; suit < kSuitCount; ++suit) {
        for (int rank = 1; rank <= kRankCount; ++rank) {
            deck.push_back(PlayingCard{.rank = rank, .suit = static_cast<Suit>(suit)});
        }
    }
    // Fisher-Yates on the raw mt19937 stream (the engine is fully specified by the
    // standard, std::shuffle's distribution is not), so a seed deals the same
    // table on every platform.
    std::mt19937 rng(seed);
    for (std::size_t i = deck.size() - 1; i > 0; --i) {
        const std::size_t j = static_cast<std::size_t>(rng()) % (i + 1);
        std::swap(deck.at(i), deck.at(j));
    }
    return deck;
}

KlondikeGame::Layout KlondikeGame::deal(std::span<const PlayingCard> deck) {
    Layout layout;
    auto next = deck.begin();
    for (int col = 0; col < kTableauCount; ++col) {
        Pile& pile = layout.tableau.at(static_cast<std::size_t>(col));
        for (int n = 0; n <= col && next != deck.end(); ++n, ++next) {
            PlayingCard card = *next;
            card.faceUp = n == col;
            pile.push_back(card);
        }
    }
    // The stock is filled bottom-up: the deck's last card ends up on top.
    for (; next != deck.end(); ++next) {
        PlayingCard card = *next;
        card.faceUp = false;
        layout.stock.push_back(card);
    }
    return layout;
}

KlondikeGame::KlondikeGame(KlondikeRules rules, std::uint32_t seed)
    : rules_(rules), layout_(deal(shuffledDeck(seed))), recordHistory_(true) {}

KlondikeGame::KlondikeGame(KlondikeRules rules, Layout layout, bool recordHistory)
    : rules_(rules), layout_(std::move(layout)), recordHistory_(recordHistory) {}

// ---- Piles ------------------------------------------------------------------

const KlondikeGame::Pile& KlondikeGame::pile(PileRef ref) const {
    return pileIn(layout_, ref);
}

KlondikeGame::Pile& KlondikeGame::pileMut(PileRef ref) {
    return pileIn(layout_, ref);
}

bool KlondikeGame::isWon() const {
    return std::ranges::all_of(layout_.foundations, [](const Pile& f) {
        return static_cast<int>(f.size()) == kRankCount;
    });
}

// ---- Moves ------------------------------------------------------------------

int KlondikeGame::sourceIndex(const Move& move) const {
    switch (move.from.kind) {
    case Kind::Stock:
        return -1;
    case Kind::Waste:
        return layout_.waste.empty() ? -1 : static_cast<int>(layout_.waste.size()) - 1;
    case Kind::Foundation: {
        if (move.from.index < 0 || move.from.index >= kFoundationCount) {
            return -1;
        }
        const Pile& f = foundation(move.from.index);
        return f.empty() ? -1 : static_cast<int>(f.size()) - 1;
    }
    case Kind::Tableau:
        break;
    }
    if (move.from.index < 0 || move.from.index >= kTableauCount) {
        return -1;
    }
    const Pile& col = tableau(move.from.index);
    if (move.fromIndex < 0 || std::cmp_greater_equal(move.fromIndex, col.size()) ||
        !col.at(static_cast<std::size_t>(move.fromIndex)).faceUp) {
        return -1;
    }
    return move.fromIndex;
}

bool KlondikeGame::accepts(PileRef to, const PlayingCard& first, int runLength) const {
    switch (to.kind) {
    case Kind::Stock:
    case Kind::Waste:
        return false;
    case Kind::Foundation: {
        if (to.index < 0 || to.index >= kFoundationCount || runLength != 1) {
            return false;
        }
        const Pile& f = foundation(to.index);
        return first.suit == foundationSuit(to.index) &&
               first.rank == static_cast<int>(f.size()) + 1;
    }
    case Kind::Tableau:
        break;
    }
    if (to.index < 0 || to.index >= kTableauCount) {
        return false;
    }
    const Pile& col = tableau(to.index);
    if (col.empty()) {
        return first.rank == kKing;
    }
    const PlayingCard& top = col.back();
    return top.faceUp && top.rank == first.rank + 1 && isRed(top.suit) != isRed(first.suit);
}

bool KlondikeGame::canMove(const Move& move) const {
    if (move.from == move.to) {
        return false;
    }
    const int index = sourceIndex(move);
    if (index < 0) {
        return false;
    }
    const Pile& src = pile(move.from);
    const int runLength = static_cast<int>(src.size()) - index;
    return accepts(move.to, src.at(static_cast<std::size_t>(index)), runLength);
}

bool KlondikeGame::move(const Move& move) {
    if (!canMove(move)) {
        return false;
    }
    pushHistory();
    const int index = sourceIndex(move);
    Pile& src = pileMut(move.from);
    Pile& dst = pileMut(move.to);
    const auto first = src.begin() + index;
    dst.insert(dst.end(), first, src.end());
    src.erase(first, src.end());

    if (move.to.kind == Kind::Foundation) {
        addScore(kScoreToFoundation);
    } else if (move.from.kind == Kind::Waste) {
        addScore(kScoreToTableau);
    } else if (move.from.kind == Kind::Foundation) {
        addScore(kScoreFromFoundation);
    }
    lastRevealed_ = false;
    if (move.from.kind == Kind::Tableau && !src.empty() && !src.back().faceUp) {
        src.back().faceUp = true;
        lastRevealed_ = true;
    }
    ++moves_;
    return true;
}

// ---- Stock ------------------------------------------------------------------

bool KlondikeGame::canRecycle() const {
    return layout_.stock.empty() && !layout_.waste.empty() &&
           (rules_.maxPasses == 0 || passes_ < rules_.maxPasses);
}

bool KlondikeGame::canDraw() const {
    return !layout_.stock.empty() || canRecycle();
}

bool KlondikeGame::draw() {
    if (!canDraw()) {
        return false;
    }
    pushHistory();
    lastRevealed_ = false;
    ++moves_;
    if (layout_.stock.empty()) {
        // Turning the waste over card by card restores the original order.
        while (!layout_.waste.empty()) {
            PlayingCard card = layout_.waste.back();
            layout_.waste.pop_back();
            card.faceUp = false;
            layout_.stock.push_back(card);
        }
        ++passes_;
        addScore(-rules_.recyclePenalty);
        return true;
    }
    for (int n = 0; n < rules_.drawCount && !layout_.stock.empty(); ++n) {
        PlayingCard card = layout_.stock.back();
        layout_.stock.pop_back();
        card.faceUp = true;
        layout_.waste.push_back(card);
    }
    return true;
}

// ---- Helpers for play ---------------------------------------------------------

std::optional<KlondikeGame::Move> KlondikeGame::autoMove(PileRef from, int fromIndex) const {
    Move candidate{.from = from, .fromIndex = fromIndex, .to = {}};
    for (int f = 0; f < kFoundationCount; ++f) {
        candidate.to = PileRef{.kind = Kind::Foundation, .index = f};
        if (canMove(candidate)) {
            return candidate;
        }
    }
    // A whole column headed by a King has nothing to gain from another empty
    // column, so such a run only goes onto a card.
    const bool wholeColumn = from.kind == Kind::Tableau && fromIndex == 0;
    for (int pass = 0; pass < 2; ++pass) {
        for (int col = 0; col < kTableauCount; ++col) {
            const bool empty = tableau(col).empty();
            if (empty != (pass == 1) || (empty && wholeColumn)) {
                continue; // occupied columns first, then empty ones
            }
            candidate.to = PileRef{.kind = Kind::Tableau, .index = col};
            if (canMove(candidate)) {
                return candidate;
            }
        }
    }
    return std::nullopt;
}

std::vector<KlondikeGame::Move> KlondikeGame::legalMoves() const {
    std::vector<Move> moves;
    const auto tryTargets = [&](PileRef from, int fromIndex, bool single) {
        Move m{.from = from, .fromIndex = fromIndex, .to = {}};
        if (single) {
            for (int f = 0; f < kFoundationCount; ++f) {
                m.to = PileRef{.kind = Kind::Foundation, .index = f};
                if (canMove(m)) {
                    moves.push_back(m);
                }
            }
        }
        for (int col = 0; col < kTableauCount; ++col) {
            m.to = PileRef{.kind = Kind::Tableau, .index = col};
            if (canMove(m)) {
                moves.push_back(m);
            }
        }
    };
    if (!layout_.waste.empty()) {
        tryTargets(PileRef{.kind = Kind::Waste, .index = 0},
                   static_cast<int>(layout_.waste.size()) - 1, true);
    }
    for (int col = 0; col < kTableauCount; ++col) {
        const Pile& pile = tableau(col);
        for (int i = 0; std::cmp_less(i, pile.size()); ++i) {
            if (pile.at(static_cast<std::size_t>(i)).faceUp) {
                tryTargets(PileRef{.kind = Kind::Tableau, .index = col}, i,
                           i == static_cast<int>(pile.size()) - 1);
            }
        }
    }
    for (int f = 0; f < kFoundationCount; ++f) {
        if (!foundation(f).empty()) {
            tryTargets(PileRef{.kind = Kind::Foundation, .index = f},
                       static_cast<int>(foundation(f).size()) - 1, false);
        }
    }
    return moves;
}

bool KlondikeGame::canAutoComplete() const {
    if (!layout_.stock.empty() || !layout_.waste.empty() || isWon()) {
        return false;
    }
    return std::ranges::all_of(layout_.tableau, [](const Pile& col) {
        return std::ranges::all_of(col, [](const PlayingCard& c) { return c.faceUp; });
    });
}

std::optional<KlondikeGame::Move> KlondikeGame::autoCompleteStep() {
    if (!canAutoComplete()) {
        return std::nullopt;
    }
    // The lowest top card that its foundation wants goes first, so the piles
    // climb evenly instead of one suit racing ahead.
    std::optional<Move> best;
    int bestRank = kRankCount + 1;
    for (int col = 0; col < kTableauCount; ++col) {
        const Pile& pile = tableau(col);
        if (pile.empty() || pile.back().rank >= bestRank) {
            continue;
        }
        const Move m{
            .from = PileRef{.kind = Kind::Tableau, .index = col},
            .fromIndex = static_cast<int>(pile.size()) - 1,
            .to = PileRef{.kind = Kind::Foundation, .index = static_cast<int>(pile.back().suit)}};
        if (canMove(m)) {
            best = m;
            bestRank = pile.back().rank;
        }
    }
    if (best && move(*best)) {
        return best;
    }
    return std::nullopt;
}

// ---- History ----------------------------------------------------------------

void KlondikeGame::pushHistory() {
    if (recordHistory_) {
        history_.push_back(
            Snapshot{.layout = layout_, .score = score_, .moves = moves_, .passes = passes_});
    }
}

bool KlondikeGame::undo() {
    if (history_.empty()) {
        return false;
    }
    Snapshot& back = history_.back();
    layout_ = std::move(back.layout);
    score_ = back.score;
    moves_ = back.moves;
    passes_ = back.passes;
    history_.pop_back();
    lastRevealed_ = false;
    return true;
}

KlondikeGame KlondikeGame::searchCopy() const {
    KlondikeGame copy(rules_, layout_, false);
    copy.score_ = score_;
    copy.moves_ = moves_;
    copy.passes_ = passes_;
    return copy;
}

void KlondikeGame::addScore(int delta) {
    score_ = std::max(0, score_ + delta);
}

// ---- Identity ---------------------------------------------------------------

std::string KlondikeGame::key() const {
    std::string key;
    key.reserve(96);
    for (const Pile& col : layout_.tableau) {
        int hidden = 0;
        for (const PlayingCard& c : col) {
            if (c.faceUp) {
                key.push_back(cardId(c));
            } else {
                ++hidden;
            }
        }
        key.push_back(static_cast<char>('0' + hidden));
        key.push_back('|');
    }
    for (const Pile& f : layout_.foundations) {
        key.push_back(static_cast<char>('0' + f.size()));
    }
    key.push_back('|');
    for (const PlayingCard& c : layout_.waste) {
        key.push_back(cardId(c));
    }
    key.push_back('|');
    for (const PlayingCard& c : layout_.stock) {
        key.push_back(cardId(c));
    }
    if (rules_.maxPasses > 0) {
        key.push_back('|');
        key.push_back(static_cast<char>('0' + passes_));
    }
    return key;
}

} // namespace og
