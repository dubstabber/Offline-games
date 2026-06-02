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
} // namespace colors

} // namespace og
