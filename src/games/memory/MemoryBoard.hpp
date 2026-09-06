#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace og {

// Board shape per difficulty: how many pairs are dealt and how the cards sit in
// rows, top to bottom. Easy and Medium use the original's staggered 3/4/4/3 and
// 3/4/4/4/4/3 layouts (the shorter rows are centered); Hard is a plain 5x6 grid.
struct MemoryLayout {
    int pairs;
    std::span<const int> rowCounts; // cards per row; sums to 2 * pairs
};

[[nodiscard]] MemoryLayout memoryLayout(int difficultyIndex);

// The pure, SDL-free Memory (concentration) table: a shuffled set of face-down
// card pairs shared by the player and a bot who alternate turns. A turn is two
// flips; resolve() then either removes a matching pair (the flipper scores and
// keeps the turn) or turns both cards back over and passes the turn. The game
// ends when every pair has been collected; whoever collected more wins.
// Determinism comes from the seeded RNG, so (pairs, seed) deals the same table.
class MemoryBoard {
public:
    enum class Player : std::uint8_t { You, Bot };
    // What resolve() did: nothing (fewer than two cards were up), removed a pair,
    // or turned a mismatched pair back over.
    enum class Outcome : std::uint8_t { None, Match, Mismatch };

    struct Card {
        int picture = 0;      // 0..kPictureCount-1; every picture appears on exactly two cards
        bool faceUp = false;  // currently revealed (and not yet resolved)
        bool matched = false; // collected; no longer on the table
    };

    // The original ships sixteen pictures; each deal draws `pairs` of them.
    static constexpr int kPictureCount = 16;
    static constexpr int kMaxPairs = kPictureCount;

    MemoryBoard(int pairs, std::uint32_t seed);

    [[nodiscard]] int pairs() const { return pairs_; }
    [[nodiscard]] int cardCount() const { return static_cast<int>(cards_.size()); }
    [[nodiscard]] const Card& card(int index) const {
        return cards_.at(static_cast<std::size_t>(index));
    }

    [[nodiscard]] Player turn() const { return turn_; }
    [[nodiscard]] int score(Player player) const {
        return player == Player::You ? youScore_ : botScore_;
    }
    [[nodiscard]] int remainingPairs() const { return pairs_ - youScore_ - botScore_; }
    [[nodiscard]] bool isOver() const { return remainingPairs() == 0; }
    // The player with more pairs once the game is over; nullopt while it is still
    // running or on a draw (only possible with an even pair count).
    [[nodiscard]] std::optional<Player> winner() const;

    // The one or two cards currently face up, in the order they were flipped.
    [[nodiscard]] std::optional<int> firstFlipped() const { return first_; }
    [[nodiscard]] std::optional<int> secondFlipped() const { return second_; }
    // Two cards are up: the next step is resolve(), not another flip.
    [[nodiscard]] bool awaitingResolve() const { return second_.has_value(); }
    [[nodiscard]] bool canFlip(int index) const;

    // Turn card `index` face up. Returns false — changing nothing — when the card
    // is off the table, already up, or two cards are already awaiting resolve().
    bool flip(int index);
    // Settle the two face-up cards (see Outcome). Match: both leave the table, the
    // current player scores and flips again. Mismatch: both turn back over and
    // the turn passes.
    Outcome resolve();

    // Deal a fresh table (new pictures and positions) and clear the scores; the
    // player moves first again.
    void reset(std::uint32_t seed);

private:
    void deal(std::uint32_t seed);

    int pairs_;
    std::vector<Card> cards_;
    std::optional<int> first_;
    std::optional<int> second_;
    Player turn_ = Player::You;
    int youScore_ = 0;
    int botScore_ = 0;
};

} // namespace og
