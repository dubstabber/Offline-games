#pragma once

#include "core/Color.hpp"

#include <cstdint>

namespace og {

// Shared difficulty level for every game. The difficulty screen lets the player
// pick one before launching a game; the game's `create` factory receives it.
// Most games offer Easy/Medium/Hard; a few (e.g. Block Fill) add a fourth,
// VeryHard. A game declares how many it uses via GameInfo::difficultyCount, so
// three-difficulty games never receive VeryHard.
enum class Difficulty : std::uint8_t { Easy, Medium, Hard, VeryHard };

// Uppercase label for the difficulty tab / selector.
[[nodiscard]] constexpr const char* label(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return "EASY";
    case Difficulty::Medium:
        return "MEDIUM";
    case Difficulty::Hard:
        return "HARD";
    case Difficulty::VeryHard:
        return "VERY HARD";
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
    case Difficulty::VeryHard:
        return colors::veryHardViolet;
    }
    return colors::text;
}

} // namespace og
