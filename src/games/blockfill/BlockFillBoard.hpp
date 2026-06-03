#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace og {

// Pure Block Fill game logic: no SDL, no rendering, fully unit-testable. The
// rendering layer (BlockFillScene) reads this and draws it. The board is an
// irregular grid: some positions are holes, the rest are playable cells. A
// single continuous "rope" starts on a fixed cell and must be dragged through
// orthogonally adjacent playable cells until it covers every playable cell
// exactly once (a Hamiltonian path). Cells are addressed by (x, y) with x the
// column and y the row.
class BlockFillBoard {
public:
    struct Cell {
        int x = 0;
        int y = 0;
        friend bool operator==(const Cell&, const Cell&) = default;
    };

    // `playable` is row-major (size width*height); non-zero = a playable cell,
    // zero = a hole. (startX, startY) is the rope's fixed start and must be a
    // playable in-bounds cell. The rope begins as just {start}.
    BlockFillBoard(int width, int height, std::span<const std::uint8_t> playable, int startX,
                   int startY);

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] int playableCount() const { return playableCount_; }
    [[nodiscard]] bool inBounds(int x, int y) const;
    [[nodiscard]] bool isPlayable(int x, int y) const; // false for holes/out-of-bounds
    [[nodiscard]] Cell start() const { return {.x = startX_, .y = startY_}; }

    // The ordered rope, start cell first. Always non-empty (begins as {start}).
    [[nodiscard]] const std::vector<Cell>& path() const { return path_; }
    [[nodiscard]] int pathLength() const { return static_cast<int>(path_.size()); }
    [[nodiscard]] Cell head() const { return path_.back(); }
    [[nodiscard]] bool pathContains(int x, int y) const; // O(1) via the on-path mask
    // Position along the rope (0 = start ... pathLength()-1 = head), or -1 if the
    // cell is not on the rope.
    [[nodiscard]] int pathIndexOf(int x, int y) const;

    [[nodiscard]] bool isSolved() const { return pathLength() == playableCount_; }

    // Advance the rope by ONE orthogonal step relative to its head:
    //  - if (x,y) is the cell directly before the head (path_[len-2]) -> retract
    //    (pop the head). Returns true.
    //  - else if (x,y) is a playable, in-bounds, orthogonal neighbour of the head
    //    that is not already on the rope -> extend (append). Returns true.
    //  - otherwise no change. Returns false.
    bool stepTo(int x, int y);

    // Drag helper: greedily take single steps toward (targetX, targetY) for as
    // long as each is a legal extend or a retract-by-one, so a fast drag that
    // skips cells still fills/retracts the cells in between. Returns the number
    // of cells added or removed.
    int dragToward(int targetX, int targetY);

    // Cut the rope back so it ends at (x, y): if that cell is on the rope, drop
    // every cell after it (truncating to it, even far from the head — tapping the
    // start cell clears the whole rope). Returns true if (x,y) was on the rope
    // (the rope now ends there); false (no change) otherwise.
    bool truncateTo(int x, int y);

    void reset(); // rope := { start }

private:
    [[nodiscard]] int index(int x, int y) const { return (y * width_) + x; }

    int width_;
    int height_;
    int startX_;
    int startY_;
    int playableCount_ = 0;
    std::vector<std::uint8_t> playable_; // row-major, non-zero = playable
    std::vector<std::uint8_t> onPath_;   // row-major membership flags (rope cells)
    std::vector<Cell> path_;
};

} // namespace og
