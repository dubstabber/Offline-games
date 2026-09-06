#include "games/mahjong/MahjongBoard.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <utility>
#include <vector>

namespace og {
namespace {

constexpr int kDealAttempts = 400;

// Groups are handed out in this order so a small set still mixes suits: the
// 1s of each suit, then the 2s, ... then honours, then flowers and seasons.
[[nodiscard]] std::vector<int> groupOrder() {
    std::vector<int> order;
    order.reserve(MahjongBoard::kGroupCount);
    for (int n = 0; n < 9; ++n) {
        order.push_back(MahjongBoard::kDots + n);
        order.push_back(MahjongBoard::kBamboo + n);
        order.push_back(MahjongBoard::kNumbers + n);
    }
    for (int g = MahjongBoard::kWinds; g < MahjongBoard::kGroupCount; ++g) {
        order.push_back(g);
    }
    return order;
}

[[nodiscard]] bool overlaps(const MahjongBoard::Slot& a, const MahjongBoard::Slot& b) {
    return std::abs(a.x - b.x) < MahjongBoard::kSpan && std::abs(a.y - b.y) < MahjongBoard::kSpan;
}

} // namespace

MahjongBoard::MahjongBoard(std::span<const Slot> slots, std::uint32_t seed) : rng_(seed) {
    const std::size_t count = slots.size() - (slots.size() % 2);
    tiles_.reserve(count);
    for (const Slot& slot : slots.subspan(0, count)) {
        tiles_.push_back(Tile{.id = tileCount(), .slot = slot});
        width_ = std::max(width_, slot.x + kSpan);
        height_ = std::max(height_, slot.y + kSpan);
        layers_ = std::max(layers_, slot.z + 1);
    }
    remaining_ = tileCount();
    computeNeighbours();

    // Pairs: two per group in hand-out order, one for a leftover half group.
    const int pairs = tileCount() / 2;
    std::vector<int> pool;
    pool.reserve(static_cast<std::size_t>(pairs));
    const std::vector<int> order = groupOrder();
    for (int p = 0; p < pairs; ++p) {
        pool.push_back(order.at(static_cast<std::size_t>((p / 2) % kGroupCount)));
    }
    std::vector<int> ids(tiles_.size());
    for (std::size_t i = 0; i < ids.size(); ++i) {
        ids.at(i) = static_cast<int>(i);
    }
    if (!dealInto(ids, pool)) {
        // Last resort: any assignment, so the board is at least playable.
        for (std::size_t i = 0; i < tiles_.size(); ++i) {
            tiles_.at(i).group = pool.at(i / 2);
        }
        solution_.clear();
    }
}

void MahjongBoard::computeNeighbours() {
    const std::size_t n = tiles_.size();
    above_.assign(n, {});
    below_.assign(n, {});
    left_.assign(n, {});
    right_.assign(n, {});
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (i == j) {
                continue;
            }
            const Slot& a = tiles_.at(i).slot;
            const Slot& b = tiles_.at(j).slot;
            if (b.z > a.z && overlaps(a, b)) {
                above_.at(i).push_back(static_cast<int>(j));
            } else if (b.z == a.z - 1 && overlaps(a, b)) {
                below_.at(i).push_back(static_cast<int>(j));
            } else if (b.z == a.z && std::abs(b.y - a.y) < kSpan) {
                if (b.x == a.x - kSpan) {
                    left_.at(i).push_back(static_cast<int>(j));
                } else if (b.x == a.x + kSpan) {
                    right_.at(i).push_back(static_cast<int>(j));
                }
            }
        }
    }
}

int MahjongBoard::roll(int n) {
    return static_cast<int>(rng_() % static_cast<std::uint32_t>(n));
}

bool MahjongBoard::isFree(int id) const {
    if (id < 0 || id >= tileCount() || tiles_.at(static_cast<std::size_t>(id)).removed) {
        return false;
    }
    const auto live = [this](int other) {
        return !tiles_.at(static_cast<std::size_t>(other)).removed;
    };
    const auto& over = above_.at(static_cast<std::size_t>(id));
    if (std::ranges::any_of(over, live)) {
        return false;
    }
    const auto& l = left_.at(static_cast<std::size_t>(id));
    const auto& r = right_.at(static_cast<std::size_t>(id));
    return std::ranges::none_of(l, live) || std::ranges::none_of(r, live);
}

bool MahjongBoard::matches(int a, int b) const {
    if (a == b || a < 0 || b < 0 || a >= tileCount() || b >= tileCount()) {
        return false;
    }
    const Tile& ta = tiles_.at(static_cast<std::size_t>(a));
    const Tile& tb = tiles_.at(static_cast<std::size_t>(b));
    return !ta.removed && !tb.removed && ta.group == tb.group;
}

void MahjongBoard::removePair(int a, int b) {
    history_.push_back(tiles_);
    tiles_.at(static_cast<std::size_t>(a)).removed = true;
    tiles_.at(static_cast<std::size_t>(b)).removed = true;
    remaining_ -= 2;
    selected_ = -1;
}

MahjongBoard::Tap MahjongBoard::tap(int id) {
    if (id < 0 || id >= tileCount() || tiles_.at(static_cast<std::size_t>(id)).removed) {
        return Tap::Ignored;
    }
    if (id == selected_) {
        selected_ = -1;
        return Tap::Deselected;
    }
    if (!isFree(id)) {
        return Tap::Blocked;
    }
    if (selected_ >= 0 && matches(selected_, id)) {
        removePair(selected_, id);
        return Tap::Matched;
    }
    selected_ = id;
    return Tap::Selected;
}

std::pair<int, int> MahjongBoard::hint() const {
    std::array<int, kGroupCount> firstFree{};
    firstFree.fill(-1);
    for (const Tile& t : tiles_) {
        if (t.removed || !isFree(t.id)) {
            continue;
        }
        int& other = firstFree.at(static_cast<std::size_t>(t.group));
        if (other >= 0) {
            return {other, t.id};
        }
        other = t.id;
    }
    return {-1, -1};
}

bool MahjongBoard::undo() {
    if (history_.empty()) {
        return false;
    }
    tiles_ = std::move(history_.back());
    history_.pop_back();
    remaining_ =
        static_cast<int>(std::ranges::count_if(tiles_, [](const Tile& t) { return !t.removed; }));
    selected_ = -1;
    return true;
}

bool MahjongBoard::shuffle() {
    if (remaining_ == 0) {
        return false;
    }
    std::vector<int> ids;
    std::array<int, kGroupCount> counts{};
    for (const Tile& t : tiles_) {
        if (!t.removed) {
            ids.push_back(t.id);
            ++counts.at(static_cast<std::size_t>(t.group));
        }
    }
    std::vector<int> pool;
    for (int g = 0; g < kGroupCount; ++g) {
        for (int c = 0; c + 1 < counts.at(static_cast<std::size_t>(g)); c += 2) {
            pool.push_back(g);
        }
    }
    const std::vector<Tile> before = tiles_;
    if (!dealInto(ids, pool)) {
        return false;
    }
    history_.push_back(before);
    selected_ = -1;
    ++shuffles_;
    return true;
}

// Would `id` still be free (nothing on it, a side clear) with `other` also down?
bool MahjongBoard::staysFree(int id, int other, const std::vector<std::uint8_t>& filled) const {
    const auto present = [&](int t) {
        return t == other || filled.at(static_cast<std::size_t>(t)) != 0;
    };
    const auto i = static_cast<std::size_t>(id);
    if (std::ranges::any_of(above_.at(i), present)) {
        return false;
    }
    return std::ranges::none_of(left_.at(i), present) ||
           std::ranges::none_of(right_.at(i), present);
}

bool MahjongBoard::placeable(int id, const std::vector<std::uint8_t>& filled) const {
    const auto i = static_cast<std::size_t>(id);
    if (filled.at(i) != 0) {
        return false;
    }
    // Only tiles taking part in this deal count; the rest are already gone.
    const auto present = [&](int other) { return filled.at(static_cast<std::size_t>(other)) != 0; };
    const auto absent = [&](int other) {
        return !tiles_.at(static_cast<std::size_t>(other)).removed && !present(other);
    };
    // Everything underneath must already be in place (it is cleared later).
    for (const int under : below_.at(i)) {
        if (absent(under)) {
            return false;
        }
    }
    const bool leftClear = std::ranges::none_of(left_.at(i), present);
    const bool rightClear = std::ranges::none_of(right_.at(i), present);
    if (!leftClear && !rightClear) {
        return false;
    }
    return !wouldTrap(id, filled, true) && !wouldTrap(id, filled, false);
}

// Placing this tile blocks one side of every not-yet-placed neighbour on that
// side, so each of them will have to leave through its far side, and so on
// down the row: if that chain ever runs into a tile already placed, one of
// them would end up walled in on both sides, and the deal would dead-end.
bool MahjongBoard::wouldTrap(int id, const std::vector<std::uint8_t>& filled, bool left) const {
    const auto& side = left ? left_ : right_;
    std::vector<int> stack;
    std::vector<std::uint8_t> seen(tiles_.size(), 0);
    for (const int n : side.at(static_cast<std::size_t>(id))) {
        if (!tiles_.at(static_cast<std::size_t>(n)).removed &&
            filled.at(static_cast<std::size_t>(n)) == 0) {
            stack.push_back(n);
        }
    }
    while (!stack.empty()) {
        const int cur = stack.back();
        stack.pop_back();
        if (seen.at(static_cast<std::size_t>(cur)) != 0) {
            continue;
        }
        seen.at(static_cast<std::size_t>(cur)) = 1;
        for (const int n : side.at(static_cast<std::size_t>(cur))) {
            if (tiles_.at(static_cast<std::size_t>(n)).removed) {
                continue;
            }
            if (filled.at(static_cast<std::size_t>(n)) != 0) {
                return true;
            }
            stack.push_back(n);
        }
    }
    return false;
}

// One attempt at laying `pairPool` down over `ids`: pairs go down in a random
// placeable order; `order` collects them (second tile first, as it comes off
// first). False when the attempt dead-ends.
bool MahjongBoard::tryDeal(std::span<const int> ids, std::span<const int> pairPool,
                           std::vector<std::pair<int, int>>& order) {
    std::vector<std::uint8_t> filled(tiles_.size(), 0);
    std::vector<int> candidates;
    order.clear();
    for (std::size_t p = 0; p < pairPool.size(); ++p) {
        std::array<int, 2> picked{-1, -1};
        for (int& slot : picked) {
            candidates.clear();
            for (const int id : ids) {
                // Both tiles of a pair are on the board when the second is
                // tapped, so the first must stay free beside/below it.
                if (placeable(id, filled) &&
                    (picked.at(0) < 0 || staysFree(picked.at(0), id, filled))) {
                    candidates.push_back(id);
                }
            }
            if (candidates.empty()) {
                return false;
            }
            slot =
                candidates.at(static_cast<std::size_t>(roll(static_cast<int>(candidates.size()))));
            filled.at(static_cast<std::size_t>(slot)) = 1;
        }
        order.emplace_back(picked.at(1), picked.at(0));
    }
    return true;
}

bool MahjongBoard::dealInto(std::span<const int> ids, std::vector<int> pairPool) {
    if (ids.size() != pairPool.size() * 2) {
        return false;
    }
    std::vector<std::pair<int, int>> order;
    for (int attempt = 0; attempt < kDealAttempts; ++attempt) {
        for (std::size_t i = pairPool.size(); i > 1; --i) {
            std::swap(pairPool.at(i - 1),
                      pairPool.at(static_cast<std::size_t>(roll(static_cast<int>(i)))));
        }
        if (!tryDeal(ids, pairPool, order)) {
            continue;
        }
        std::array<int, kGroupCount> faces{};
        for (std::size_t p = 0; p < order.size(); ++p) {
            const int group = pairPool.at(p);
            for (const int id : {order.at(p).first, order.at(p).second}) {
                Tile& t = tiles_.at(static_cast<std::size_t>(id));
                t.group = group;
                t.face = (group == kFlowers || group == kSeasons)
                             ? faces.at(static_cast<std::size_t>(group))++ % kCopies
                             : 0;
            }
        }
        solution_.assign(order.rbegin(), order.rend());
        return true;
    }
    return false;
}

} // namespace og
