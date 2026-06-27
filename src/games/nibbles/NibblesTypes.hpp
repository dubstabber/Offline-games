#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace og::nibbles {

enum class Direction : std::uint8_t { Up, Right, Down, Left };
enum class Cell : std::uint8_t { Empty, Wall, Warp };

struct Position {
    int x = 0;
    int y = 0;

    [[nodiscard]] friend constexpr bool operator==(Position a, Position b) {
        return a.x == b.x && a.y == b.y;
    }
};

struct Spawn {
    Position pos;
    Direction direction = Direction::Right;
};

struct Warp {
    int id = 0;
    Position source; // top-left of the 2x2 source portal
    Position target;
    bool random = false;
    bool bidirectional = false;
};

struct NibblesLevel {
    int width = 0;
    int height = 0;
    int sourceLevel = 0;
    std::vector<Cell> cells;
    std::vector<Spawn> spawns;
    std::vector<Warp> warps;
};

inline constexpr std::array<Direction, 4> kDirections{Direction::Up, Direction::Right,
                                                      Direction::Down, Direction::Left};

[[nodiscard]] constexpr Direction opposite(Direction direction) {
    switch (direction) {
    case Direction::Up:
        return Direction::Down;
    case Direction::Right:
        return Direction::Left;
    case Direction::Down:
        return Direction::Up;
    case Direction::Left:
        return Direction::Right;
    }
    return Direction::Right;
}

[[nodiscard]] constexpr Direction turnLeft(Direction direction) {
    switch (direction) {
    case Direction::Up:
        return Direction::Left;
    case Direction::Right:
        return Direction::Up;
    case Direction::Down:
        return Direction::Right;
    case Direction::Left:
        return Direction::Down;
    }
    return Direction::Right;
}

[[nodiscard]] constexpr Direction turnRight(Direction direction) {
    switch (direction) {
    case Direction::Up:
        return Direction::Right;
    case Direction::Right:
        return Direction::Down;
    case Direction::Down:
        return Direction::Left;
    case Direction::Left:
        return Direction::Up;
    }
    return Direction::Right;
}

[[nodiscard]] constexpr Position delta(Direction direction) {
    switch (direction) {
    case Direction::Up:
        return {.x = 0, .y = -1};
    case Direction::Right:
        return {.x = 1, .y = 0};
    case Direction::Down:
        return {.x = 0, .y = 1};
    case Direction::Left:
        return {.x = -1, .y = 0};
    }
    return {};
}

[[nodiscard]] constexpr Position add(Position a, Position b) {
    return {.x = a.x + b.x, .y = a.y + b.y};
}

} // namespace og::nibbles
