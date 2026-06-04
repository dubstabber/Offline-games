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

// Linear blend a -> b by t in [0,1], keeping a's alpha. Used for the per-hex top
// gradient and for tinting ground toward a player's color along an active trail.
[[nodiscard]] constexpr Color mix(Color a, Color b, float t) {
    const auto ch = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(static_cast<float>(x) +
                                         ((static_cast<float>(y) - static_cast<float>(x)) * t));
    };
    return rgb(ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), a.a);
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

// Unclaimed ground + the thin honeycomb grid look. Ground hexes are drawn nearly
// full-size over a darker backdrop, so the backdrop shows through the slim gaps as
// the grid grooves between cells. A faint top-to-bottom gradient on each hex face
// (kGroundTop lit -> kGroundBottom shadowed) gives the empty field a subtle bevel
// like the reference art instead of reading as a flat slab.
inline constexpr Color kGround = rgb(50, 52, 60);
inline constexpr Color kGroundTop = rgb(56, 58, 67);
inline constexpr Color kGroundBottom = rgb(41, 43, 51);
inline constexpr Color kBackdrop = rgb(24, 25, 31);

// Out-of-bounds filler honeycomb: drawn for visible cells beyond the play grid so
// the screen never shows a hard black void at the map edge. Kept clearly darker
// than kGround so the actual board boundary still reads as a faint rim.
inline constexpr Color kVoid = rgb(31, 32, 39);

// Side-wall shading factors applied to the territory's top color (a vertical
// gradient from the lit top edge down to the shadowed base).
inline constexpr float kWallTop = 0.74F;
inline constexpr float kWallBottom = 0.50F;

// Top-face shading: claimed/trail hex tops are drawn with a slight gradient from a
// lit north edge (kFaceTop) down to a faintly darker south edge (kFaceBottom),
// matching the bevelled look of the ground hexes.
inline constexpr float kFaceTop = 1.06F;
inline constexpr float kFaceBottom = 0.90F;

// Multiply a color's channels by f, clamped to [0,255] so highlights don't wrap.
[[nodiscard]] constexpr Color shade(Color c, float f) {
    const auto ch = [f](std::uint8_t v) {
        const float x = static_cast<float>(v) * f;
        return static_cast<std::uint8_t>(x > 255.0F ? 255.0F : x);
    };
    return rgb(ch(c.r), ch(c.g), ch(c.b), c.a);
}

} // namespace og::hexanaut::palette
