#pragma once

#include "core/Color.hpp"
#include "games/hexanaut/HexTypes.hpp"

#include <array>
#include <cstdint>

// Colors for Hexanaut's territories, ground, and prism shading. These are the
// only place the game names colors; the simulation itself carries player *ids*,
// not colors, so it stays SDL-free. Territory colors are identity colors (they
// do not flip with dark mode).
namespace og::hexanaut::palette {

[[nodiscard]] constexpr Color darken(Color c, float f) {
    return rgb(static_cast<std::uint8_t>(static_cast<float>(c.r) * f),
               static_cast<std::uint8_t>(static_cast<float>(c.g) * f),
               static_cast<std::uint8_t>(static_cast<float>(c.b) * f), c.a);
}

[[nodiscard]] constexpr Color withAlpha(Color c, std::uint8_t a) {
    return rgb(c.r, c.g, c.b, a);
}

// Blend a color toward white by t in [0,1] — used to make active trails glow.
[[nodiscard]] constexpr Color lighten(Color c, float t) {
    const auto mix = [t](std::uint8_t v) {
        return static_cast<std::uint8_t>(static_cast<float>(v) +
                                         ((255.0F - static_cast<float>(v)) * t));
    };
    return rgb(mix(c.r), mix(c.g), mix(c.b), c.a);
}

// Per-player territory base colors (the prism top face). Index 0 is the human.
inline constexpr std::array<Color, 8> kPlayerColors{{
    rgb(225, 64, 196),  // 0 human  - magenta
    rgb(231, 76, 76),   // 1 red
    rgb(54, 140, 240),  // 2 blue
    rgb(76, 187, 122),  // 3 green
    rgb(240, 165, 80),  // 4 orange
    rgb(150, 110, 230), // 5 purple
    rgb(48, 190, 190),  // 6 teal
    rgb(236, 200, 70),  // 7 yellow
}};

[[nodiscard]] constexpr Color topColor(PlayerId p) {
    return kPlayerColors.at(static_cast<std::size_t>(p) % kPlayerColors.size());
}

// Unclaimed ground + the thin grid look (achieved by insetting ground hexes so
// the darker background shows through as gaps).
inline constexpr Color kGround = rgb(48, 51, 60);
inline constexpr Color kBackdrop = rgb(30, 32, 39);

// Side-wall shading factors applied to the territory's top color (a vertical
// gradient from the lit top edge down to the shadowed base).
inline constexpr float kWallTop = 0.74F;
inline constexpr float kWallBottom = 0.50F;

} // namespace og::hexanaut::palette
