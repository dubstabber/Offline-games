#include "games/memory/MemoryBot.hpp"

#include <algorithm>
#include <cstddef>

namespace og {
namespace {

constexpr int kUnknown = -1;

// Face down and still on the table: the only cards a flip may target.
[[nodiscard]] bool onTable(const MemoryBoard& board, int index) {
    const MemoryBoard::Card& c = board.card(index);
    return !c.matched && !c.faceUp;
}

} // namespace

MemoryBot::MemoryBot(MemoryBotProfile profile, std::uint32_t seed)
    : profile_(profile), rng_(seed) {}

void MemoryBot::reset() {
    memory_.clear();
}

bool MemoryBot::knows(int index) const {
    return index >= 0 && static_cast<std::size_t>(index) < memory_.size() &&
           memory_.at(static_cast<std::size_t>(index)) != kUnknown;
}

int MemoryBot::knownCount() const {
    return static_cast<int>(std::count_if(memory_.begin(), memory_.end(),
                                          [](int picture) { return picture != kUnknown; }));
}

float MemoryBot::recallProbability(const MemoryBoard& board) const {
    using Player = MemoryBoard::Player;
    const int lead = board.score(Player::You) - board.score(Player::Bot);
    const float t = std::clamp(
        0.5F + (static_cast<float>(lead) / (2.0F * static_cast<float>(kExtremeLead))), 0.0F, 1.0F);
    return profile_.recallLow + ((profile_.recallHigh - profile_.recallLow) * t);
}

void MemoryBot::observe(const MemoryBoard& board, int index) {
    if (index < 0 || index >= board.cardCount()) {
        return;
    }
    if (memory_.size() != static_cast<std::size_t>(board.cardCount())) {
        memory_.assign(static_cast<std::size_t>(board.cardCount()), kUnknown);
    }
    int& slot = memory_.at(static_cast<std::size_t>(index));
    if (slot != kUnknown) {
        return; // already remembered; seeing it again never makes it forget
    }
    std::uniform_real_distribution<float> unit(0.0F, 1.0F);
    if (unit(rng_) < recallProbability(board)) {
        slot = board.card(index).picture;
    }
}

int MemoryBot::rememberedPartner(const MemoryBoard& board, int index) const {
    const int picture = board.card(index).picture;
    for (int i = 0; i < board.cardCount(); ++i) {
        if (i != index && knows(i) && memory_.at(static_cast<std::size_t>(i)) == picture &&
            onTable(board, i)) {
            return i;
        }
    }
    return kUnknown;
}

std::vector<int> MemoryBot::unseenCards(const MemoryBoard& board, int exclude) const {
    std::vector<int> out;
    for (int i = 0; i < board.cardCount(); ++i) {
        if (i != exclude && onTable(board, i) && !knows(i)) {
            out.push_back(i);
        }
    }
    return out;
}

std::vector<int> MemoryBot::tableCards(const MemoryBoard& board, int exclude) {
    std::vector<int> out;
    for (int i = 0; i < board.cardCount(); ++i) {
        if (i != exclude && onTable(board, i)) {
            out.push_back(i);
        }
    }
    return out;
}

int MemoryBot::pick(const std::vector<int>& candidates) {
    if (candidates.empty()) {
        return kUnknown;
    }
    std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
    return candidates.at(dist(rng_));
}

int MemoryBot::chooseFirst(const MemoryBoard& board) {
    // A remembered pair wins outright.
    for (int i = 0; i < board.cardCount(); ++i) {
        if (knows(i) && onTable(board, i) && rememberedPartner(board, i) != kUnknown) {
            return i;
        }
    }
    // Otherwise explore: an unseen card can only add to what the bot knows. If
    // every table card were remembered a pair would be among them, so this is
    // only empty for a malformed table; fall back to any card then.
    const int unseen = pick(unseenCards(board, kUnknown));
    return unseen != kUnknown ? unseen : pick(tableCards(board, kUnknown));
}

int MemoryBot::chooseSecond(const MemoryBoard& board, int first) {
    const int partner = rememberedPartner(board, first);
    if (partner != kUnknown) {
        return partner;
    }
    const int unseen = pick(unseenCards(board, first));
    return unseen != kUnknown ? unseen : pick(tableCards(board, first));
}

} // namespace og
