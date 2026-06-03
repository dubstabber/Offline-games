#pragma once

#include <array>
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

// Tap Match: maroon field, grey fruit tiles (white/raised when free, grey when
// covered), and a dark holder bar with recessed slots.
constexpr Color tapMatchMaroon = rgb(122, 48, 66);      // background
constexpr Color tapMatchTileLight = rgb(246, 245, 249); // accessible (free) tile card
constexpr Color tapMatchTileDim = rgb(197, 194, 203);   // covered tile card
constexpr Color tapMatchTileEdge = rgb(158, 150, 162);  // tile border/separation
constexpr Color tapMatchHolder = rgb(96, 38, 52);       // holder bar panel
constexpr Color tapMatchSlot = rgb(78, 30, 44);         // recessed holder slot

// Minesweeper: the classic adjacent-mine number colors (1..8), chosen to read on
// the dark slate field, plus the shovel/flag toggle's active green and idle slate
// (the green is the original's flagButtonColorWhenFlagging). Identity colors:
// they do not flip with the theme.
constexpr std::array<Color, 8> mineNumbers = {
    rgb(54, 118, 240),  // 1 blue
    rgb(64, 168, 78),   // 2 green
    rgb(232, 80, 80),   // 3 red
    rgb(150, 110, 230), // 4 purple
    rgb(214, 140, 70),  // 5 amber
    rgb(48, 190, 190),  // 6 teal
    rgb(210, 214, 224), // 7 light grey
    rgb(150, 156, 170), // 8 grey
};
constexpr Color mineToggleActive = rgb(118, 202, 62); // selected dig/flag mode
constexpr Color mineToggleIdle = rgb(96, 104, 120);   // unselected mode

constexpr Color white = rgb(255, 255, 255);
constexpr Color overlay = rgb(0, 0, 0, 150); // semi-transparent game-over layer
} // namespace colors

} // namespace og
