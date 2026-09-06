#include "games/twentyfortyeight/TwentyFortyEightBoard.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <random>

namespace og {
namespace {

constexpr int kFourPercent = 10; // chance a spawned tile is a 4 rather than a 2

} // namespace

TwentyFortyEightBoard::TwentyFortyEightBoard(int size, std::uint32_t seed)
    : size_(std::clamp(size, kMinSize, kMaxSize)), rng_(seed) {
    reset();
}

void TwentyFortyEightBoard::reset() {
    cells_.fill(0);
    score_ = 0;
    canUndo_ = false;
    spawnTile(nullptr);
    spawnTile(nullptr);
}

void TwentyFortyEightBoard::clearForTest() {
    cells_.fill(0);
    score_ = 0;
    canUndo_ = false;
}

int TwentyFortyEightBoard::maxTile() const {
    int best = 0;
    for (int y = 0; y < size_; ++y) {
        for (int x = 0; x < size_; ++x) {
            best = std::max(best, at(x, y));
        }
    }
    return best;
}

int TwentyFortyEightBoard::emptyCount() const {
    int count = 0;
    for (int y = 0; y < size_; ++y) {
        for (int x = 0; x < size_; ++x) {
            if (at(x, y) == 0) {
                ++count;
            }
        }
    }
    return count;
}

bool TwentyFortyEightBoard::canMove() const {
    if (emptyCount() > 0) {
        return true;
    }
    // Full board: only an equal neighbor pair (right or down covers every pair)
    // still allows a merge.
    for (int y = 0; y < size_; ++y) {
        for (int x = 0; x < size_; ++x) {
            const int v = at(x, y);
            if ((x + 1 < size_ && at(x + 1, y) == v) || (y + 1 < size_ && at(x, y + 1) == v)) {
                return true;
            }
        }
    }
    return false;
}

TwentyFortyEightBoard::Pos TwentyFortyEightBoard::cellAlong(Direction dir, int line, int k) const {
    const int far = size_ - 1;
    switch (dir) {
    case Direction::Left:
        return {.x = k, .y = line};
    case Direction::Right:
        return {.x = far - k, .y = line};
    case Direction::Up:
        return {.x = line, .y = k};
    case Direction::Down:
        return {.x = line, .y = far - k};
    }
    return {.x = k, .y = line};
}

bool TwentyFortyEightBoard::slideLine(Direction dir, int line, Cells& next, MoveResult* out,
                                      int& gained) const {
    // Gather the line's tiles in travel order (index 0 = the wall they slide to).
    std::array<int, kMaxSize> vals{};
    std::array<int, kMaxSize> src{};
    int count = 0;
    for (int k = 0; k < size_; ++k) {
        const Pos p = cellAlong(dir, line, k);
        const int v = at(p.x, p.y);
        if (v != 0) {
            vals.at(static_cast<std::size_t>(count)) = v;
            src.at(static_cast<std::size_t>(count)) = k;
            ++count;
        }
    }
    const auto record = [&](int fromK, int toK, int value, bool merged) {
        if (out == nullptr) {
            return;
        }
        const Pos from = cellAlong(dir, line, fromK);
        const Pos to = cellAlong(dir, line, toK);
        out->tiles.push_back({.fromX = from.x,
                              .fromY = from.y,
                              .toX = to.x,
                              .toY = to.y,
                              .value = value,
                              .merged = merged});
    };
    // Pack toward the wall, merging each equal pair once (a merged tile never
    // merges again in the same move because the scan moves past both).
    bool moved = false;
    int outK = 0;
    for (int i = 0; i < count; ++outK) {
        const auto ii = static_cast<std::size_t>(i);
        const Pos to = cellAlong(dir, line, outK);
        const bool pair = i + 1 < count && vals.at(ii) == vals.at(ii + 1);
        if (pair) {
            const int merged = vals.at(ii) * 2;
            next.at(index(to.x, to.y)) = merged;
            gained += merged;
            moved = true;
            record(src.at(ii), outK, vals.at(ii), true);
            record(src.at(ii + 1), outK, vals.at(ii + 1), true);
            i += 2;
        } else {
            next.at(index(to.x, to.y)) = vals.at(ii);
            if (src.at(ii) != outK) {
                moved = true;
            }
            record(src.at(ii), outK, vals.at(ii), false);
            i += 1;
        }
    }
    return moved;
}

bool TwentyFortyEightBoard::move(Direction dir, MoveResult* out) {
    if (out != nullptr) {
        *out = MoveResult{};
    }
    Cells next{};
    int gained = 0;
    bool moved = false;
    for (int line = 0; line < size_; ++line) {
        if (slideLine(dir, line, next, out, gained)) {
            moved = true;
        }
    }
    if (!moved) {
        if (out != nullptr) {
            out->tiles.clear(); // nothing happened: no motion to animate
        }
        return false;
    }
    undoCells_ = cells_;
    undoScore_ = score_;
    canUndo_ = true;
    cells_ = next;
    score_ += gained;
    if (out != nullptr) {
        out->gained = gained;
    }
    spawnTile(out);
    return true;
}

bool TwentyFortyEightBoard::undo() {
    if (!canUndo_) {
        return false;
    }
    cells_ = undoCells_;
    score_ = undoScore_;
    canUndo_ = false;
    return true;
}

void TwentyFortyEightBoard::spawnTile(MoveResult* out) {
    std::array<std::size_t, kMaxCells> empties{};
    int count = 0;
    const std::size_t total = static_cast<std::size_t>(size_) * static_cast<std::size_t>(size_);
    for (std::size_t i = 0; i < total; ++i) {
        if (cells_.at(i) == 0) {
            empties.at(static_cast<std::size_t>(count)) = i;
            ++count;
        }
    }
    if (count == 0) {
        return;
    }
    const auto pick =
        static_cast<std::size_t>(std::uniform_int_distribution<int>(0, count - 1)(rng_));
    const std::size_t cell = empties.at(pick);
    const int value = std::uniform_int_distribution<int>(0, 99)(rng_) < kFourPercent ? 4 : 2;
    cells_.at(cell) = value;
    if (out != nullptr) {
        out->spawnX = static_cast<int>(cell) % size_;
        out->spawnY = static_cast<int>(cell) / size_;
        out->spawnValue = value;
    }
}

} // namespace og
