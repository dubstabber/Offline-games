#include "games/arrows/ArrowsBoard.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace og {

ArrowsBoard::ArrowsBoard(int width, int height, std::vector<Arrow> arrows)
    : width_(std::max(1, width)), height_(std::max(1, height)), arrows_(std::move(arrows)),
      removed_(arrows_.size(), 0),
      occupant_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), -1),
      remaining_(static_cast<int>(arrows_.size())) {
    for (int i = 0; i < arrowCount(); ++i) {
        place(i);
    }
}

bool ArrowsBoard::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

bool ArrowsBoard::isRemoved(int index) const {
    return index < 0 || index >= arrowCount() || removed_.at(static_cast<std::size_t>(index)) != 0;
}

int ArrowsBoard::arrowAt(int x, int y) const {
    if (!inBounds(x, y)) {
        return -1;
    }
    return occupant_.at(static_cast<std::size_t>(cellIndex(x, y)));
}

void ArrowsBoard::place(int index) {
    for (const ArrowCell& c : arrows_.at(static_cast<std::size_t>(index)).cells) {
        if (inBounds(c.x, c.y)) {
            occupant_.at(static_cast<std::size_t>(cellIndex(c.x, c.y))) = index;
        }
    }
}

void ArrowsBoard::clear(int index) {
    for (const ArrowCell& c : arrows_.at(static_cast<std::size_t>(index)).cells) {
        if (inBounds(c.x, c.y) && arrowAt(c.x, c.y) == index) {
            occupant_.at(static_cast<std::size_t>(cellIndex(c.x, c.y))) = -1;
        }
    }
    removed_.at(static_cast<std::size_t>(index)) = 1;
    --remaining_;
}

int ArrowsBoard::blockedAfter(int index) const {
    if (isRemoved(index)) {
        return -1;
    }
    const Arrow& arrow = arrows_.at(static_cast<std::size_t>(index));
    int x = arrow.cells.back().x + dirDx(arrow.dir);
    int y = arrow.cells.back().y + dirDy(arrow.dir);
    int steps = 0;
    while (inBounds(x, y)) {
        if (arrowAt(x, y) >= 0) {
            return steps;
        }
        ++steps;
        x += dirDx(arrow.dir);
        y += dirDy(arrow.dir);
    }
    return -1;
}

int ArrowsBoard::blockerOf(int index) const {
    if (isRemoved(index)) {
        return -1;
    }
    const Arrow& arrow = arrows_.at(static_cast<std::size_t>(index));
    int x = arrow.cells.back().x + dirDx(arrow.dir);
    int y = arrow.cells.back().y + dirDy(arrow.dir);
    while (inBounds(x, y)) {
        const int other = arrowAt(x, y);
        if (other >= 0) {
            return other;
        }
        x += dirDx(arrow.dir);
        y += dirDy(arrow.dir);
    }
    return -1;
}

ArrowsBoard::Tap ArrowsBoard::tap(int index) {
    if (isRemoved(index) || isWon() || isLost()) {
        return Tap::Ignored;
    }
    if (isFree(index)) {
        clear(index);
        return Tap::Cleared;
    }
    --hearts_;
    return Tap::Blocked;
}

void ArrowsBoard::reset() {
    std::ranges::fill(occupant_, -1);
    std::ranges::fill(removed_, 0);
    remaining_ = arrowCount();
    hearts_ = kHearts;
    for (int i = 0; i < arrowCount(); ++i) {
        place(i);
    }
}

bool ArrowsBoard::isSolvable() const {
    ArrowsBoard copy = *this;
    bool progressed = true;
    while (progressed && copy.remaining_ > 0) {
        progressed = false;
        for (int i = 0; i < copy.arrowCount(); ++i) {
            if (!copy.isRemoved(i) && copy.isFree(i)) {
                copy.clear(i);
                progressed = true;
            }
        }
    }
    return copy.remaining_ == 0;
}

} // namespace og
