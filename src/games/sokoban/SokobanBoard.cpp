#include "games/sokoban/SokobanBoard.hpp"

#include <algorithm>
#include <cstddef>

namespace og {
namespace {

[[nodiscard]] int safeDimension(int value) {
    return std::max(1, value);
}

[[nodiscard]] std::size_t cellCount(int width, int height) {
    return static_cast<std::size_t>(safeDimension(width)) *
           static_cast<std::size_t>(safeDimension(height));
}

[[nodiscard]] bool isGoalChar(char cell) {
    return cell == '.' || cell == '*' || cell == '+';
}

[[nodiscard]] bool isBoxChar(char cell) {
    return cell == '$' || cell == '*';
}

[[nodiscard]] bool isPlayerChar(char cell) {
    return cell == '@' || cell == '+';
}

} // namespace

SokobanBoard::SokobanBoard(int width, int height, std::span<const char> cells)
    : width_(safeDimension(width)), height_(safeDimension(height)),
      walls_(cellCount(width_, height_), 0), goals_(cellCount(width_, height_), 0),
      boxes_(cellCount(width_, height_), 0), initialBoxes_(cellCount(width_, height_), 0) {
    bool foundPlayer = false;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto idx = static_cast<std::size_t>(index(x, y));
            char cell = ' ';
            if (idx < cells.size()) {
                // std::span has no bounds-checked accessor in C++20; the guard above
                // keeps this one indexed read in range.
                cell = cells
                    [idx]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            }
            if (cell == '#') {
                walls_.at(idx) = 1;
            }
            if (isGoalChar(cell)) {
                goals_.at(idx) = 1;
                ++goalCount_;
            }
            if (isBoxChar(cell)) {
                boxes_.at(idx) = 1;
                ++boxCount_;
            }
            if (isPlayerChar(cell) && !foundPlayer) {
                player_ = {.x = x, .y = y};
                foundPlayer = true;
            }
        }
    }
    initialBoxes_ = boxes_;
    initialPlayer_ = player_;
}

bool SokobanBoard::inBounds(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

bool SokobanBoard::isWall(int x, int y) const {
    return !inBounds(x, y) || walls_.at(static_cast<std::size_t>(index(x, y))) != 0;
}

bool SokobanBoard::isGoal(int x, int y) const {
    return inBounds(x, y) && goals_.at(static_cast<std::size_t>(index(x, y))) != 0;
}

bool SokobanBoard::hasBox(int x, int y) const {
    return inBounds(x, y) && boxes_.at(static_cast<std::size_t>(index(x, y))) != 0;
}

int SokobanBoard::boxesOnGoals() const {
    int count = 0;
    for (std::size_t i = 0; i < boxes_.size(); ++i) {
        if (boxes_.at(i) != 0 && goals_.at(i) != 0) {
            ++count;
        }
    }
    return count;
}

bool SokobanBoard::isSolved() const {
    return boxCount_ > 0 && boxCount_ == goalCount_ && boxesOnGoals() == boxCount_;
}

SokobanBoard::Cell SokobanBoard::moved(Cell cell, Direction direction) {
    switch (direction) {
    case Direction::Up:
        --cell.y;
        break;
    case Direction::Down:
        ++cell.y;
        break;
    case Direction::Left:
        --cell.x;
        break;
    case Direction::Right:
        ++cell.x;
        break;
    }
    return cell;
}

void SokobanBoard::pushSnapshot() {
    history_.push_back(
        Snapshot{.player = player_, .boxes = boxes_, .moves = moves_, .pushes = pushes_});
}

void SokobanBoard::restore(const Snapshot& snapshot) {
    player_ = snapshot.player;
    boxes_ = snapshot.boxes;
    moves_ = snapshot.moves;
    pushes_ = snapshot.pushes;
}

bool SokobanBoard::tryMove(Direction direction) {
    const Cell next = moved(player_, direction);
    if (isWall(next.x, next.y)) {
        return false;
    }

    if (hasBox(next.x, next.y)) {
        const Cell pushed = moved(next, direction);
        if (isWall(pushed.x, pushed.y) || hasBox(pushed.x, pushed.y)) {
            return false;
        }
        pushSnapshot();
        boxes_.at(static_cast<std::size_t>(index(next.x, next.y))) = 0;
        boxes_.at(static_cast<std::size_t>(index(pushed.x, pushed.y))) = 1;
        player_ = next;
        ++moves_;
        ++pushes_;
        return true;
    }

    pushSnapshot();
    player_ = next;
    ++moves_;
    return true;
}

bool SokobanBoard::undo() {
    if (history_.empty()) {
        return false;
    }
    restore(history_.back());
    history_.pop_back();
    return true;
}

void SokobanBoard::reset() {
    boxes_ = initialBoxes_;
    player_ = initialPlayer_;
    moves_ = 0;
    pushes_ = 0;
    history_.clear();
}

} // namespace og
