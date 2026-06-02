#pragma once

#include <cstdint>
#include <SDL3/SDL.h>

namespace og {

// Thin alias so call sites read as plain data; SDL_Color is {r,g,b,a} bytes.
using Color = SDL_Color;

[[nodiscard]] constexpr Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b,
                                  std::uint8_t a = 255) {
    return Color{r, g, b, a};
}

// Shared palette. The whole app is text + emoji on flat code-drawn shapes, so a
// small, named palette keeps every scene visually consistent.
namespace colors {
constexpr Color background = rgb(24, 26, 32);
constexpr Color surface = rgb(38, 42, 52);
constexpr Color surfaceAlt = rgb(52, 58, 72);
constexpr Color accent = rgb(99, 179, 237);
constexpr Color text = rgb(232, 234, 240);
constexpr Color textMuted = rgb(150, 156, 170);
constexpr Color line = rgb(80, 86, 100);

// JindoBlu-style theme used by the menu, difficulty screen, and Tic-Tac-Toe.
constexpr Color coral = rgb(240, 138, 110);       // Tic-Tac-Toe background
constexpr Color panelBrown = rgb(74, 54, 54);     // scoreboard panel
constexpr Color youRed = rgb(231, 76, 76);        // player (YOU / X)
constexpr Color botCyan = rgb(45, 196, 230);      // bot (BOT / O)
constexpr Color gridBlack = rgb(28, 28, 30);      // board grid lines
constexpr Color cream = rgb(250, 235, 215);       // menu / difficulty background
constexpr Color easyGreen = rgb(76, 187, 122);    // difficulty: easy
constexpr Color mediumOrange = rgb(240, 165, 80); // difficulty: medium
constexpr Color hardRed = rgb(231, 76, 76);       // difficulty: hard
constexpr Color menuPink = rgb(236, 122, 150);    // menu card accent / top-bar icon
constexpr Color menuPurple = rgb(150, 120, 220);  // menu card accent
constexpr Color menuYellow = rgb(240, 188, 70);   // menu card accent
constexpr Color white = rgb(255, 255, 255);
constexpr Color overlay = rgb(0, 0, 0, 150); // semi-transparent game-over layer
} // namespace colors

} // namespace og
