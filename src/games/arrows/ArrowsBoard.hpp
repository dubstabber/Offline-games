#pragma once

#include <cstdint>
#include <vector>

namespace og {

// Which way an arrow's head points; also the direction it slides when tapped.
enum class ArrowDir : std::uint8_t { Up, Right, Down, Left };

[[nodiscard]] constexpr int dirDx(ArrowDir d) {
    switch (d) {
    case ArrowDir::Right:
        return 1;
    case ArrowDir::Left:
        return -1;
    case ArrowDir::Up:
    case ArrowDir::Down:
        return 0;
    }
    return 0;
}
[[nodiscard]] constexpr int dirDy(ArrowDir d) {
    switch (d) {
    case ArrowDir::Down:
        return 1;
    case ArrowDir::Up:
        return -1;
    case ArrowDir::Left:
    case ArrowDir::Right:
        return 0;
    }
    return 0;
}
[[nodiscard]] constexpr ArrowDir opposite(ArrowDir d) {
    switch (d) {
    case ArrowDir::Up:
        return ArrowDir::Down;
    case ArrowDir::Right:
        return ArrowDir::Left;
    case ArrowDir::Down:
        return ArrowDir::Up;
    case ArrowDir::Left:
        return ArrowDir::Right;
    }
    return ArrowDir::Up;
}

struct ArrowCell {
    int x = 0;
    int y = 0;
    friend bool operator==(const ArrowCell&, const ArrowCell&) = default;
};

// One arrow: a simple orthogonal path of cells from its tail to its head, plus
// the way the head points. For a path of two or more cells the head direction
// is the step from the second-to-last cell to the head; a single-cell arrow
// carries its direction only here.
struct Arrow {
    std::vector<ArrowCell> cells; // tail first, head last; never empty
    ArrowDir dir = ArrowDir::Up;
};

// Pure Arrows puzzle logic: no SDL, no rendering, fully unit-testable. The
// rendering layer (ArrowsScene) reads this and draws it.
//
// The board is a grid where every cell is covered by at most one arrow.
// Tapping an arrow slides it forward like a train: the head travels straight
// on in its direction and the body follows the path, so the arrow leaves the
// board when every cell straight ahead of the head, up to the edge, is empty.
// If another arrow lies in the way the tap fails and costs a heart. Clear every
// arrow to win; lose every heart and the level is lost. Clearing an arrow only
// ever frees cells, so an arrow that is free stays free: a board that the
// generator built solvable can never be played into a dead end, only into
// running out of hearts.
class ArrowsBoard {
public:
    static constexpr int kHearts = 3;

    enum class Tap : std::uint8_t {
        Cleared, // the arrow slid off the board
        Blocked, // something was in the way: one heart lost
        Ignored, // no such live arrow, or the level is already over
    };

    ArrowsBoard(int width, int height, std::vector<Arrow> arrows);

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] bool inBounds(int x, int y) const;

    // Every arrow the level started with, cleared ones included (their index
    // stays stable so the scene can animate them out); check isRemoved().
    [[nodiscard]] const std::vector<Arrow>& arrows() const { return arrows_; }
    [[nodiscard]] int arrowCount() const { return static_cast<int>(arrows_.size()); }
    [[nodiscard]] bool isRemoved(int index) const;
    [[nodiscard]] int remaining() const { return remaining_; }
    [[nodiscard]] int hearts() const { return hearts_; }

    // The live arrow covering (x, y), or -1 for an empty / out-of-bounds cell.
    [[nodiscard]] int arrowAt(int x, int y) const;

    // How far the arrow's head can travel: the number of empty cells straight
    // ahead before the first covered one, or -1 when the run reaches the edge
    // (the arrow is free to leave).
    [[nodiscard]] int blockedAfter(int index) const;
    // The live arrow standing in this arrow's way, or -1 when it is free.
    [[nodiscard]] int blockerOf(int index) const;
    [[nodiscard]] bool isFree(int index) const { return blockedAfter(index) < 0; }

    Tap tap(int index);

    [[nodiscard]] bool isWon() const { return remaining_ == 0; }
    [[nodiscard]] bool isLost() const { return hearts_ <= 0 && !isWon(); }

    // Put every arrow back and restore the hearts.
    void reset();

    // Whether repeatedly clearing free arrows empties the board from the
    // current position (evaluated on a copy; the board is left untouched).
    [[nodiscard]] bool isSolvable() const;

private:
    [[nodiscard]] int cellIndex(int x, int y) const { return (y * width_) + x; }
    void place(int index);
    void clear(int index); // remove a live arrow from the grid

    int width_;
    int height_;
    std::vector<Arrow> arrows_;
    std::vector<std::uint8_t> removed_; // per arrow
    std::vector<int> occupant_;         // row-major: live arrow index or -1
    int remaining_ = 0;
    int hearts_ = kHearts;
};

} // namespace og
