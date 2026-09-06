#include "games/memory/MemoryBoard.hpp"

#include <algorithm>
#include <array>
#include <numeric>
#include <random>

namespace og {
namespace {

constexpr std::array<int, 4> kEasyRows{3, 4, 4, 3};         // 14 cards
constexpr std::array<int, 6> kMediumRows{3, 4, 4, 4, 4, 3}; // 22 cards
constexpr std::array<int, 6> kHardRows{5, 5, 5, 5, 5, 5};   // 30 cards

} // namespace

MemoryLayout memoryLayout(int difficultyIndex) {
    switch (difficultyIndex) {
    case 0:
        return {.pairs = 7, .rowCounts = kEasyRows};
    case 2:
        return {.pairs = 15, .rowCounts = kHardRows};
    default:
        return {.pairs = 11, .rowCounts = kMediumRows};
    }
}

MemoryBoard::MemoryBoard(int pairs, std::uint32_t seed) : pairs_(std::clamp(pairs, 1, kMaxPairs)) {
    deal(seed);
}

void MemoryBoard::deal(std::uint32_t seed) {
    std::mt19937 rng(seed);
    // Draw `pairs` distinct pictures, then lay each down twice and shuffle.
    std::array<int, kPictureCount> pictures{};
    std::iota(pictures.begin(), pictures.end(), 0);
    std::shuffle(pictures.begin(), pictures.end(), rng);

    cards_.clear();
    cards_.reserve(static_cast<std::size_t>(pairs_) * 2U);
    for (int i = 0; i < pairs_; ++i) {
        const int picture = pictures.at(static_cast<std::size_t>(i));
        cards_.push_back(Card{.picture = picture});
        cards_.push_back(Card{.picture = picture});
    }
    std::shuffle(cards_.begin(), cards_.end(), rng);
}

void MemoryBoard::reset(std::uint32_t seed) {
    deal(seed);
    first_.reset();
    second_.reset();
    turn_ = Player::You;
    youScore_ = 0;
    botScore_ = 0;
}

std::optional<MemoryBoard::Player> MemoryBoard::winner() const {
    if (!isOver() || youScore_ == botScore_) {
        return std::nullopt;
    }
    return youScore_ > botScore_ ? Player::You : Player::Bot;
}

bool MemoryBoard::canFlip(int index) const {
    if (index < 0 || index >= cardCount() || awaitingResolve()) {
        return false;
    }
    const Card& c = card(index);
    return !c.matched && !c.faceUp;
}

bool MemoryBoard::flip(int index) {
    if (!canFlip(index)) {
        return false;
    }
    cards_.at(static_cast<std::size_t>(index)).faceUp = true;
    if (first_) {
        second_ = index;
    } else {
        first_ = index;
    }
    return true;
}

MemoryBoard::Outcome MemoryBoard::resolve() {
    if (!first_ || !second_) {
        return Outcome::None;
    }
    Card& a = cards_.at(static_cast<std::size_t>(*first_));
    Card& b = cards_.at(static_cast<std::size_t>(*second_));
    a.faceUp = false;
    b.faceUp = false;
    first_.reset();
    second_.reset();
    if (a.picture == b.picture) {
        a.matched = true;
        b.matched = true;
        (turn_ == Player::You ? youScore_ : botScore_) += 1;
        return Outcome::Match; // the matcher keeps the turn
    }
    turn_ = turn_ == Player::You ? Player::Bot : Player::You;
    return Outcome::Mismatch;
}

} // namespace og
