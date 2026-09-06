#pragma once

#include "games/Difficulty.hpp"
#include "games/memory/MemoryBoard.hpp"

#include <cstdint>
#include <random>
#include <vector>

namespace og {

// How reliably the bot memorises each card it sees. Like the original, the
// bot rubber-bands with the score: it remembers with `recallLow` probability
// when it is well ahead and `recallHigh` when the player is, interpolating in
// between (see MemoryBot::kExtremeLead).
struct MemoryBotProfile {
    float recallLow;
    float recallHigh;
};

[[nodiscard]] constexpr MemoryBotProfile memoryBotProfile(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return {.recallLow = 0.05F, .recallHigh = 0.35F};
    case Difficulty::Hard:
    case Difficulty::VeryHard:
        return {.recallLow = 0.55F, .recallHigh = 0.95F};
    case Difficulty::Medium:
        break;
    }
    return {.recallLow = 0.25F, .recallHigh = 0.65F};
}

// Pure (SDL-free) opponent for Memory. It watches every card either side turns
// over and commits each to memory with the profile's recall probability. On its
// turn it flips a remembered pair when it has one; otherwise it explores an
// unknown card and, if it remembers that picture's partner, takes it. The RNG
// is seedable so the random tiers are unit-testable.
class MemoryBot {
public:
    // Lead (in pairs) at which the recall probability sits at an end of the
    // profile's range: the player this far ahead makes the bot its sharpest.
    static constexpr int kExtremeLead = 4;

    MemoryBot(MemoryBotProfile profile, std::uint32_t seed);

    // A card was turned face up (by either side): maybe remember it.
    void observe(const MemoryBoard& board, int index);
    // The bot's first flip of a turn: a remembered pair if it has one, else a
    // card it has not seen. Assumes at least one pair is still on the table.
    [[nodiscard]] int chooseFirst(const MemoryBoard& board);
    // The bot's second flip given its first (already face up): the remembered
    // partner when it knows it, else an unseen card.
    [[nodiscard]] int chooseSecond(const MemoryBoard& board, int first);
    // Wipe the memory for a new deal.
    void reset();

    [[nodiscard]] bool knows(int index) const;
    [[nodiscard]] int knownCount() const;
    // The chance the next observed card is remembered, given the score gap.
    [[nodiscard]] float recallProbability(const MemoryBoard& board) const;

private:
    // Remembered partner of the face-down, unmatched card `index` (or -1).
    [[nodiscard]] int rememberedPartner(const MemoryBoard& board, int index) const;
    // A random card from `candidates`, or -1 when it is empty.
    [[nodiscard]] int pick(const std::vector<int>& candidates);
    [[nodiscard]] std::vector<int> unseenCards(const MemoryBoard& board, int exclude) const;
    [[nodiscard]] static std::vector<int> tableCards(const MemoryBoard& board, int exclude);

    MemoryBotProfile profile_;
    std::mt19937 rng_;
    std::vector<int> memory_; // memory_[card] = its picture, or -1 when forgotten
};

} // namespace og
