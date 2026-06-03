#include "games/blockfill/BlockFillBoard.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

namespace og {
namespace {

// -1, 0, or +1 for a negative, zero, or positive value.
[[nodiscard]] int sign(int v) {
    if (v > 0) {
        return 1;
    }
    if (v < 0) {
        return -1;
    }
    return 0;
}

} // namespace

BlockFillBoard::BlockFillBoard(int width, int height, std::span<const std::uint8_t> playable,
                               int startX, int startY)
    : width_(width), height_(height), startX_(startX), startY_(startY),
      playable_(playable.begin(), playable.end()),
      onPath_(static_cast<std::size_t>(width * height), 0) {
    for (const std::uint8_t cell : playable_) {
        if (cell != 0) {
            ++playableCount_;
        }
    }
    reset();
}

bool BlockFillBoard::inBounds(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

bool BlockFillBoard::isPlayable(int x, int y) const {
    return inBounds(x, y) && playable_.at(static_cast<std::size_t>(index(x, y))) != 0;
}

bool BlockFillBoard::pathContains(int x, int y) const {
    return inBounds(x, y) && onPath_.at(static_cast<std::size_t>(index(x, y))) != 0;
}

int BlockFillBoard::pathIndexOf(int x, int y) const {
    if (!pathContains(x, y)) {
        return -1;
    }
    for (std::size_t i = 0; i < path_.size(); ++i) {
        if (path_.at(i).x == x && path_.at(i).y == y) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void BlockFillBoard::reset() {
    std::ranges::fill(onPath_, std::uint8_t{0});
    path_.clear();
    path_.push_back({.x = startX_, .y = startY_});
    if (inBounds(startX_, startY_)) {
        onPath_.at(static_cast<std::size_t>(index(startX_, startY_))) = 1;
    }
}

bool BlockFillBoard::stepTo(int x, int y) {
    if (!inBounds(x, y)) {
        return false;
    }
    const Cell h = head();
    if (std::abs(h.x - x) + std::abs(h.y - y) != 1) {
        return false; // only orthogonal single steps
    }
    // Retract: stepping back onto the cell just before the head pops the head.
    if (pathLength() >= 2) {
        const Cell prev = path_.at(path_.size() - 2);
        if (prev.x == x && prev.y == y) {
            onPath_.at(static_cast<std::size_t>(index(h.x, h.y))) = 0;
            path_.pop_back();
            return true;
        }
    }
    // Extend: onto a fresh playable neighbour.
    if (isPlayable(x, y) && !pathContains(x, y)) {
        onPath_.at(static_cast<std::size_t>(index(x, y))) = 1;
        path_.push_back({.x = x, .y = y});
        return true;
    }
    return false;
}

bool BlockFillBoard::truncateTo(int x, int y) {
    const int idx = pathIndexOf(x, y);
    if (idx < 0) {
        return false; // not on the rope
    }
    while (pathLength() > idx + 1) {
        const Cell tail = path_.back();
        onPath_.at(static_cast<std::size_t>(index(tail.x, tail.y))) = 0;
        path_.pop_back();
    }
    return true;
}

int BlockFillBoard::dragToward(int targetX, int targetY) {
    int changed = 0;
    // Bounded so a malformed target can never spin: at most every cell once each
    // way. Each iteration either makes progress (stepTo true) or stops.
    for (int guard = (playableCount_ * 2) + 2; guard > 0; --guard) {
        const Cell h = head();
        if (h.x == targetX && h.y == targetY) {
            break;
        }
        const int dx = targetX - h.x;
        const int dy = targetY - h.y;
        const int sx = sign(dx);
        const int sy = sign(dy);
        // Try the dominant axis first, then the other, so we move one orthogonal
        // cell toward the target.
        bool moved = false;
        if (std::abs(dx) >= std::abs(dy)) {
            moved = (sx != 0 && stepTo(h.x + sx, h.y)) || (sy != 0 && stepTo(h.x, h.y + sy));
        } else {
            moved = (sy != 0 && stepTo(h.x, h.y + sy)) || (sx != 0 && stepTo(h.x + sx, h.y));
        }
        if (!moved) {
            break;
        }
        ++changed;
    }
    return changed;
}

} // namespace og
