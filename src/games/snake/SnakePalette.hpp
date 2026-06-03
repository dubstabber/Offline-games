#pragma once

#include "core/Color.hpp"
#include "games/snake/SnakeConfig.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Snake-specific colors (food orbs + per-snake body gradients). Kept in the
// snake module rather than the shared colors:: palette so the simulation stays
// SDL-free — only SnakeScene (the renderer) includes this. Food colors are the
// original's 10 hues (SnakesSimulatorConfig.asset FoodColors, 0..1 -> 0..255);
// gradients approximate the original's per-skin two-stop gradients (index 0 is
// the player's magenta->blue, like the demo screenshot).
namespace og::snake::palette {

inline constexpr std::array<Color, config::kFoodColorCount> kFoodColors{{
    rgb(10, 254, 200), // cyan
    rgb(248, 69, 89),  // red
    rgb(255, 247, 1),  // yellow
    rgb(239, 56, 178), // pink
    rgb(0, 113, 255),  // blue
    rgb(255, 176, 0),  // orange
    rgb(164, 64, 255), // purple
    rgb(190, 248, 29), // lime
    rgb(0, 194, 240),  // cyan-blue
    rgb(37, 231, 0),   // lime-green
}};

struct Gradient {
    Color head;
    Color tail;
};

inline constexpr std::array<Gradient, config::kSnakeGradientCount> kSnakeGradients{{
    {.head = rgb(240, 70, 160), .tail = rgb(70, 120, 240)},  // 0 player: magenta->blue
    {.head = rgb(96, 222, 120), .tail = rgb(30, 150, 90)},   // 1 greens (Evan)
    {.head = rgb(250, 180, 40), .tail = rgb(240, 90, 60)},   // 2 orange->red
    {.head = rgb(150, 110, 235), .tail = rgb(232, 96, 220)}, // 3 purple->magenta
    {.head = rgb(46, 205, 222), .tail = rgb(46, 120, 232)},  // 4 cyan->blue
    {.head = rgb(244, 222, 64), .tail = rgb(124, 202, 44)},  // 5 yellow->lime
    {.head = rgb(232, 84, 84), .tail = rgb(124, 44, 164)},   // 6 red->purple
    {.head = rgb(64, 212, 182), .tail = rgb(44, 96, 204)},   // 7 teal->blue
    {.head = rgb(250, 144, 204), .tail = rgb(182, 84, 232)}, // 8 pink->violet
    {.head = rgb(184, 230, 96), .tail = rgb(64, 162, 124)},  // 9 lime->teal
}};

// Linear blend between a gradient's head (t=0) and tail (t=1) color.
[[nodiscard]] inline Color sampleGradient(std::uint8_t gradIndex, float t) {
    const Gradient& g = kSnakeGradients.at(gradIndex % kSnakeGradients.size());
    const float u = std::clamp(t, 0.0F, 1.0F);
    const auto blend = [u](std::uint8_t a, std::uint8_t b) {
        const auto fa = static_cast<float>(a);
        const auto fb = static_cast<float>(b);
        return static_cast<std::uint8_t>(std::lround(fa + ((fb - fa) * u)));
    };
    return rgb(blend(g.head.r, g.tail.r), blend(g.head.g, g.tail.g), blend(g.head.b, g.tail.b));
}

[[nodiscard]] inline Color foodColor(std::uint8_t index) {
    return kFoodColors.at(index % kFoodColors.size());
}

} // namespace og::snake::palette
