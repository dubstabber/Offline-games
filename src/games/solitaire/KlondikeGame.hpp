#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace og {

enum class Suit : std::uint8_t { Spades, Hearts, Diamonds, Clubs };
inline constexpr int kSuitCount = 4;
inline constexpr int kRankCount = 13; // Ace (1) .. King (13)

[[nodiscard]] constexpr bool isRed(Suit suit) {
    return suit == Suit::Hearts || suit == Suit::Diamonds;
}

struct PlayingCard {
    int rank = 1; // 1 = Ace .. 13 = King
    Suit suit = Suit::Spades;
    bool faceUp = false;

    [[nodiscard]] constexpr bool operator==(const PlayingCard&) const = default;
};

// The same card regardless of which way up it is.
[[nodiscard]] constexpr bool sameCard(const PlayingCard& a, const PlayingCard& b) {
    return a.rank == b.rank && a.suit == b.suit;
}

// Per-difficulty rules: how many cards a stock tap turns over, how many passes
// through the stock are allowed (0 = unlimited), and what turning the waste back
// into the stock costs.
struct KlondikeRules {
    int drawCount;
    int maxPasses;
    int recyclePenalty;
};

[[nodiscard]] constexpr KlondikeRules klondikeRules(int difficultyIndex) {
    switch (difficultyIndex) {
    case 0:
        return {.drawCount = 1, .maxPasses = 0, .recyclePenalty = 0};
    case 2:
        return {.drawCount = 3, .maxPasses = 3, .recyclePenalty = 100};
    default:
        return {.drawCount = 3, .maxPasses = 0, .recyclePenalty = 50};
    }
}

// One of the table's piles. Foundations are 0..3 (one per Suit, in enum order),
// tableau columns 0..6.
struct PileRef {
    enum class Kind : std::uint8_t { Stock, Waste, Foundation, Tableau };
    Kind kind = Kind::Stock;
    int index = 0;

    [[nodiscard]] constexpr bool operator==(const PileRef&) const = default;
};

// The pure, SDL-free Klondike solitaire table. Seven tableau columns are dealt
// 1..7 cards with the last face up; the remaining 24 form the stock. Cards move
// as face-up runs: onto a tableau card of the opposite color and one rank higher
// (only a King onto an empty column), or singly onto their suit's foundation in
// rank order from the Ace. Tapping the stock turns drawCount cards onto the
// waste; an empty stock takes the waste back (a new pass) while passes remain.
// Every move and draw is recorded so undo() can step back. The deal is a pure
// function of the seed on every platform, so seed lists can be shipped.
class KlondikeGame {
public:
    static constexpr int kTableauCount = 7;
    static constexpr int kFoundationCount = 4;
    static constexpr int kDeckSize = 52;

    using Pile = std::vector<PlayingCard>;

    // Every pile on the table; the test/solver constructor takes one directly.
    struct Layout {
        std::array<Pile, kTableauCount> tableau;
        std::array<Pile, kFoundationCount> foundations;
        Pile stock; // back() is the top card
        Pile waste; // back() is the top (playable) card

        [[nodiscard]] bool operator==(const Layout&) const = default;
    };

    // Moving a run: everything from `fromIndex` to the end of a tableau column, or
    // the top card of the waste / a foundation (fromIndex is ignored for those).
    struct Move {
        PileRef from;
        int fromIndex = 0;
        PileRef to;

        [[nodiscard]] constexpr bool operator==(const Move&) const = default;
    };

    // Score awards, in the spirit of the original: a card leaving the waste for a
    // column, any card reaching a foundation, and taking one back off it.
    static constexpr int kScoreToTableau = 5;
    static constexpr int kScoreToFoundation = 10;
    static constexpr int kScoreFromFoundation = -10;

    KlondikeGame(KlondikeRules rules, std::uint32_t seed);
    // Start from an explicit table. `recordHistory` off makes copies cheap for
    // the solver (undo() is then unavailable).
    KlondikeGame(KlondikeRules rules, Layout layout, bool recordHistory = true);

    // The 52 cards in a platform-independent shuffled order for `seed`.
    [[nodiscard]] static std::vector<PlayingCard> shuffledDeck(std::uint32_t seed);
    // The standard deal of a 52-card deck: columns 0..6 take 1..7 cards (the
    // last of each face up), the remaining 24 are the stock.
    [[nodiscard]] static Layout deal(std::span<const PlayingCard> deck);
    [[nodiscard]] static constexpr Suit foundationSuit(int index) {
        return static_cast<Suit>(index);
    }

    [[nodiscard]] const KlondikeRules& rules() const { return rules_; }
    [[nodiscard]] const Pile& stock() const { return layout_.stock; }
    [[nodiscard]] const Pile& waste() const { return layout_.waste; }
    [[nodiscard]] const Pile& foundation(int index) const {
        return layout_.foundations.at(static_cast<std::size_t>(index));
    }
    [[nodiscard]] const Pile& tableau(int index) const {
        return layout_.tableau.at(static_cast<std::size_t>(index));
    }
    [[nodiscard]] const Pile& pile(PileRef ref) const;
    [[nodiscard]] const Layout& layout() const { return layout_; }

    [[nodiscard]] int score() const { return score_; }
    [[nodiscard]] int moves() const { return moves_; }
    // Passes through the stock so far (the deal is the first).
    [[nodiscard]] int passes() const { return passes_; }
    [[nodiscard]] bool isWon() const;

    [[nodiscard]] bool canMove(const Move& move) const;
    // Apply a legal move: the run changes pile, the score updates, and a tableau
    // card left uncovered turns face up. Returns false (changing nothing) when
    // the move is illegal.
    bool move(const Move& move);
    // Whether the last move() turned a tableau card face up.
    [[nodiscard]] bool lastMoveRevealed() const { return lastRevealed_; }

    // Tap the stock: turn cards onto the waste, or take the waste back.
    [[nodiscard]] bool canDraw() const;
    [[nodiscard]] bool canRecycle() const;
    bool draw();

    // Where a tap on the run at (from, fromIndex) should send it: its foundation
    // when it can go there, else the first column that accepts it.
    [[nodiscard]] std::optional<Move> autoMove(PileRef from, int fromIndex) const;
    // Every legal move (excluding draws), source piles in table order.
    [[nodiscard]] std::vector<Move> legalMoves() const;

    [[nodiscard]] bool canUndo() const { return !history_.empty(); }
    bool undo();

    // This position with no history and recording off: the light copy the
    // solver branches from (score, moves and passes carry over).
    [[nodiscard]] KlondikeGame searchCopy() const;

    // Nothing is hidden and the stock is spent: the rest is a formality that
    // autoCompleteStep() plays one card at a time.
    [[nodiscard]] bool canAutoComplete() const;
    std::optional<Move> autoCompleteStep();

    // A compact identity of the position for the solver's visited set.
    [[nodiscard]] std::string key() const;

private:
    struct Snapshot {
        Layout layout;
        int score;
        int moves;
        int passes;
    };

    [[nodiscard]] Pile& pileMut(PileRef ref);
    // The index of the first moved card for `move`, or -1 if the source is empty
    // or the move names a card that cannot be taken.
    [[nodiscard]] int sourceIndex(const Move& move) const;
    [[nodiscard]] bool accepts(PileRef to, const PlayingCard& first, int runLength) const;
    void pushHistory();
    void addScore(int delta);

    KlondikeRules rules_;
    Layout layout_;
    int score_ = 0;
    int moves_ = 0;
    int passes_ = 1;
    bool lastRevealed_ = false;
    bool recordHistory_;
    std::vector<Snapshot> history_;
};

} // namespace og
