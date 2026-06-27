#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace og {

// Pure Sokoban rules: no SDL, no rendering, fully unit-testable. Static board
// cells are floor/wall/goal; boxes and the player are dynamic. A valid move
// advances the player one orthogonal cell, optionally pushing one box if the
// square beyond it is free.
class SokobanBoard {
public:
    struct Cell {
        int x = 0;
        int y = 0;
        friend bool operator==(const Cell&, const Cell&) = default;
    };

    enum class Direction : std::uint8_t { Up, Down, Left, Right };

    SokobanBoard(int width, int height, std::span<const char> cells);

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] Cell player() const { return player_; }
    [[nodiscard]] int moves() const { return moves_; }
    [[nodiscard]] int pushes() const { return pushes_; }
    [[nodiscard]] int boxCount() const { return boxCount_; }
    [[nodiscard]] int goalCount() const { return goalCount_; }
    [[nodiscard]] bool canUndo() const { return !history_.empty(); }

    [[nodiscard]] bool inBounds(int x, int y) const;
    [[nodiscard]] bool isWall(int x, int y) const;
    [[nodiscard]] bool isGoal(int x, int y) const;
    [[nodiscard]] bool hasBox(int x, int y) const;
    [[nodiscard]] int boxesOnGoals() const;
    [[nodiscard]] bool isSolved() const;

    bool tryMove(Direction direction);
    bool undo();
    void reset();

private:
    struct Snapshot {
        Cell player{};
        std::vector<std::uint8_t> boxes;
        int moves = 0;
        int pushes = 0;
    };

    [[nodiscard]] int index(int x, int y) const { return (y * width_) + x; }
    [[nodiscard]] static Cell moved(Cell cell, Direction direction);
    void pushSnapshot();
    void restore(const Snapshot& snapshot);

    int width_ = 1;
    int height_ = 1;
    std::vector<std::uint8_t> walls_;
    std::vector<std::uint8_t> goals_;
    std::vector<std::uint8_t> boxes_;
    std::vector<std::uint8_t> initialBoxes_;
    Cell player_{};
    Cell initialPlayer_{};
    int moves_ = 0;
    int pushes_ = 0;
    int boxCount_ = 0;
    int goalCount_ = 0;
    std::vector<Snapshot> history_;
};

} // namespace og
