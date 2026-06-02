#pragma once

#include "core/Color.hpp"

#include <cstdint>

namespace og {

// Shared difficulty level for every game. The difficulty screen lets the player
// pick one before launching a game; the game's `create` factory receives it.
enum class Difficulty : std::uint8_t { Easy, Medium, Hard };

// Uppercase label for the difficulty tab / selector.
[[nodiscard]] constexpr const char* label(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return "EASY";
    case Difficulty::Medium:
        return "MEDIUM";
    case Difficulty::Hard:
        return "HARD";
    }
    return "";
}

// Theme color used for the difficulty label, slider fill, and PLAY button.
[[nodiscard]] constexpr Color color(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return colors::easyGreen;
    case Difficulty::Medium:
        return colors::mediumOrange;
    case Difficulty::Hard:
        return colors::hardRed;
    }
    return colors::text;
}

} // namespace og
