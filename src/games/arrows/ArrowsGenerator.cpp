#include "games/arrows/ArrowsGenerator.hpp"

#include "games/arrows/ArrowsBoard.hpp"
#include "games/Difficulty.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <random>
#include <utility>
#include <vector>

namespace og {
namespace {

constexpr std::array<ArrowDir, 4> kDirs{ArrowDir::Up, ArrowDir::Right, ArrowDir::Down,
                                        ArrowDir::Left};

// Levels pack tighter as the player progresses, up to this many extra percent.
constexpr int kFillRampMax = 12;
// How often a growing body carries straight on rather than turning (percent).
constexpr int kStraightPercent = 70;

// A 32-bit integer hash (the lowbias32 mix) so neighbouring levels land on
// unrelated seeds.
[[nodiscard]] std::uint32_t mix32(std::uint32_t x) {
    x ^= x >> 16U;
    x *= 0x7feb352dU;
    x ^= x >> 15U;
    x *= 0x846ca68bU;
    x ^= x >> 16U;
    return x;
}

// Grows arrows onto a grid one at a time while maintaining a removal order
// that clears them all (see generateArrowsBoard). Every arrow keeps a position
// in that order; a new arrow is placed at the first position after every arrow
// standing in its run to the edge, which is only valid if every arrow whose
// own run it interrupts comes later still.
class Builder {
public:
    Builder(const ArrowsLevelSpec& spec, std::uint32_t seed)
        : spec_(spec), rng_(seed),
          occupant_(static_cast<std::size_t>(spec.width) * static_cast<std::size_t>(spec.height),
                    kEmpty),
          reserved_(occupant_.size(), 0) {}

    [[nodiscard]] std::vector<Arrow> build() {
        const int cells = spec_.width * spec_.height;
        const int target = (cells * spec_.fillPercent) / 100;
        // Rounds: each round visits every empty cell in a fresh random order,
        // so the fill only stops when no cell anywhere can start an arrow. A
        // one-cell arrow is a last resort for the gaps that are left once no
        // longer arrow fits anywhere.
        for (const int minCells : {2, 1}) {
            bool added = true;
            while (added && covered_ < target) {
                added = false;
                for (const ArrowCell& c : shuffledEmptyCells()) {
                    if (covered_ >= target) {
                        break;
                    }
                    added = tryAddArrow(c.x, c.y, minCells) || added;
                }
            }
        }
        // Hand the arrows over in removal order (first to clear first), which
        // is a convenient order for anything that wants a hint.
        std::vector<Arrow> ordered;
        ordered.reserve(arrows_.size());
        for (const int i : order_) {
            ordered.push_back(std::move(arrows_.at(static_cast<std::size_t>(i))));
        }
        return ordered;
    }

private:
    static constexpr int kEmpty = -1;

    // Uniform in [0, n) from the raw engine stream, so results do not depend
    // on a library's distribution implementation.
    [[nodiscard]] int roll(int n) {
        return static_cast<int>(rng_() % static_cast<std::uint32_t>(n));
    }

    [[nodiscard]] std::vector<ArrowCell> shuffledEmptyCells() {
        std::vector<ArrowCell> empties;
        for (int y = 0; y < spec_.height; ++y) {
            for (int x = 0; x < spec_.width; ++x) {
                if (occupant_.at(index(x, y)) == kEmpty) {
                    empties.push_back({.x = x, .y = y});
                }
            }
        }
        for (std::size_t i = empties.size(); i > 1; --i) {
            std::swap(empties.at(i - 1),
                      empties.at(static_cast<std::size_t>(roll(static_cast<int>(i)))));
        }
        return empties;
    }

    [[nodiscard]] bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < spec_.width && y < spec_.height;
    }
    [[nodiscard]] std::size_t index(int x, int y) const {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(spec_.width)) +
               static_cast<std::size_t>(x);
    }
    [[nodiscard]] bool isFreeForBody(int x, int y) const {
        return inBounds(x, y) && occupant_.at(index(x, y)) == kEmpty &&
               reserved_.at(index(x, y)) == 0;
    }
    [[nodiscard]] int positionOf(int arrow) const {
        return pos_.at(static_cast<std::size_t>(arrow));
    }

    // The run straight ahead of the head up to the edge: reserve its empty
    // cells so the body is never laid across them, and return the latest
    // removal position among the arrows standing in it (-1 if none).
    int reserveRun(int headX, int headY, ArrowDir dir) {
        int latest = -1;
        int x = headX + dirDx(dir);
        int y = headY + dirDy(dir);
        while (inBounds(x, y)) {
            const int other = occupant_.at(index(x, y));
            if (other == kEmpty) {
                reserved_.at(index(x, y)) = 1;
            } else {
                latest = std::max(latest, positionOf(other));
            }
            x += dirDx(dir);
            y += dirDy(dir);
        }
        return latest;
    }

    void releaseRun(int headX, int headY, ArrowDir dir) {
        int x = headX + dirDx(dir);
        int y = headY + dirDy(dir);
        while (inBounds(x, y)) {
            reserved_.at(index(x, y)) = 0;
            x += dirDx(dir);
            y += dirDy(dir);
        }
    }

    // Walk the body backwards from the head: the first step must go straight
    // back (so the last segment lines up with the head), later steps mostly
    // carry on straight and sometimes turn, never crossing anything. Cells are
    // claimed for `pending` as they are taken.
    [[nodiscard]] std::vector<ArrowCell> growBody(int headX, int headY, ArrowDir dir, int wanted,
                                                  int pending) {
        std::vector<ArrowCell> body{{.x = headX, .y = headY}};
        occupant_.at(index(headX, headY)) = pending;
        ArrowDir step = opposite(dir);
        while (std::cmp_less(body.size(), wanted)) {
            const ArrowCell& cur = body.back();
            ArrowDir next = step;
            const bool straightOk = isFreeForBody(cur.x + dirDx(step), cur.y + dirDy(step));
            const bool keepStraight = body.size() == 1 || roll(100) < kStraightPercent;
            if (!(straightOk && keepStraight)) {
                std::array<ArrowDir, 4> options{};
                std::size_t count = 0;
                for (const ArrowDir d : kDirs) {
                    if (d != opposite(step) && isFreeForBody(cur.x + dirDx(d), cur.y + dirDy(d))) {
                        options.at(count++) = d;
                    }
                }
                if (count == 0 || body.size() == 1) {
                    break; // boxed in (a first step can only go straight back)
                }
                next = options.at(static_cast<std::size_t>(roll(static_cast<int>(count))));
            }
            body.push_back({.x = cur.x + dirDx(next), .y = cur.y + dirDy(next)});
            occupant_.at(index(body.back().x, body.back().y)) = pending;
            step = next;
        }
        return body;
    }

    // The earliest removal position among the existing arrows whose line to
    // the edge passes through the new body (they can only leave once it is
    // gone). Every head on the line through a body cell that faces back along
    // it counts, whatever else stands in between: those in-between arrows will
    // be cleared eventually, and then the new body would be next in the way.
    [[nodiscard]] int earliestInterrupted(const std::vector<ArrowCell>& body, int pending) const {
        int earliest = std::numeric_limits<int>::max();
        for (const ArrowCell& c : body) {
            for (const ArrowDir d : kDirs) {
                int x = c.x + dirDx(d);
                int y = c.y + dirDy(d);
                while (inBounds(x, y)) {
                    const int other = occupant_.at(index(x, y));
                    if (other != kEmpty && other != pending) {
                        const Arrow& arrow = arrows_.at(static_cast<std::size_t>(other));
                        if (arrow.cells.back() == ArrowCell{.x = x, .y = y} &&
                            arrow.dir == opposite(d)) {
                            earliest = std::min(earliest, positionOf(other));
                        }
                    }
                    x += dirDx(d);
                    y += dirDy(d);
                }
            }
        }
        return earliest;
    }

    void discard(const std::vector<ArrowCell>& body) {
        for (const ArrowCell& c : body) {
            occupant_.at(index(c.x, c.y)) = kEmpty;
        }
    }

    bool tryAddArrow(int x, int y, int minCells) {
        if (occupant_.at(index(x, y)) != kEmpty) {
            return false;
        }
        std::array<ArrowDir, 4> dirs = kDirs;
        for (std::size_t i = dirs.size() - 1; i > 0; --i) {
            std::swap(dirs.at(i), dirs.at(static_cast<std::size_t>(roll(static_cast<int>(i) + 1))));
        }
        const int pending = static_cast<int>(arrows_.size());
        for (const ArrowDir dir : dirs) {
            const int latestInWay = reserveRun(x, y, dir);
            const int wanted = spec_.minLength + roll(spec_.maxLength - spec_.minLength + 1);
            std::vector<ArrowCell> body = growBody(x, y, dir, std::max(1, wanted), pending);
            releaseRun(x, y, dir);
            // Everything in the way must go before this arrow, and everything
            // whose run it interrupts must go after it.
            if (std::cmp_less(body.size(), minCells) ||
                earliestInterrupted(body, pending) <= latestInWay) {
                discard(body);
                continue;
            }
            covered_ += static_cast<int>(body.size());
            std::ranges::reverse(body); // tail first, head last
            arrows_.push_back(Arrow{.cells = std::move(body), .dir = dir});
            order_.insert(order_.begin() + latestInWay + 1, pending);
            pos_.assign(arrows_.size(), 0);
            for (std::size_t p = 0; p < order_.size(); ++p) {
                pos_.at(static_cast<std::size_t>(order_.at(p))) = static_cast<int>(p);
            }
            return true;
        }
        return false;
    }

    ArrowsLevelSpec spec_;
    std::mt19937 rng_;
    std::vector<int> occupant_;          // row-major: arrow index or kEmpty
    std::vector<std::uint8_t> reserved_; // the run of the arrow being grown
    std::vector<Arrow> arrows_;          // in creation order
    std::vector<int> order_;             // arrow indices in removal order
    std::vector<int> pos_;               // per arrow: its index in order_
    int covered_ = 0;
};

} // namespace

ArrowsLevelSpec arrowsLevelSpec(Difficulty difficulty, int level) {
    ArrowsLevelSpec spec;
    switch (difficulty) {
    case Difficulty::Easy:
        spec = {.width = 8, .height = 12, .minLength = 3, .maxLength = 10, .fillPercent = 55};
        break;
    case Difficulty::Medium:
        spec = {.width = 10, .height = 15, .minLength = 4, .maxLength = 14, .fillPercent = 68};
        break;
    case Difficulty::Hard:
    case Difficulty::VeryHard:
        spec = {.width = 13, .height = 19, .minLength = 4, .maxLength = 20, .fillPercent = 80};
        break;
    }
    spec.fillPercent += std::min(kFillRampMax, std::max(0, level - 1) / 2);
    return spec;
}

std::uint32_t arrowsLevelSeed(Difficulty difficulty, int level) {
    const auto tier = static_cast<std::uint32_t>(difficulty);
    return mix32(0x41525257U ^ (tier << 24U) ^ static_cast<std::uint32_t>(std::max(1, level)));
}

ArrowsBoard generateArrowsBoard(const ArrowsLevelSpec& spec, std::uint32_t seed) {
    ArrowsLevelSpec safe = spec;
    safe.width = std::max(1, spec.width);
    safe.height = std::max(1, spec.height);
    safe.minLength = std::max(1, spec.minLength);
    safe.maxLength = std::max(safe.minLength, spec.maxLength);
    safe.fillPercent = std::clamp(spec.fillPercent, 0, 100);
    Builder builder(safe, seed);
    return {safe.width, safe.height, builder.build()};
}

ArrowsBoard arrowsBoardFor(Difficulty difficulty, int level) {
    return generateArrowsBoard(arrowsLevelSpec(difficulty, level),
                               arrowsLevelSeed(difficulty, level));
}

} // namespace og
