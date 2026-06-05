#pragma once

#include <algorithm>

// Small, pure easing/tween helpers shared by animated scenes. Header-only, so
// any game can include it without a CMake entry. Each takes a normalized
// progress t in [0, 1] and returns the eased progress (easeOutBack briefly
// overshoots past 1). Lifted verbatim from the original Tap Match scene so the
// animation feel is unchanged; now reusable by every game.
namespace og::ease {

[[nodiscard]] constexpr float clampUnit(float v) {
    return std::clamp(v, 0.0F, 1.0F);
}

[[nodiscard]] constexpr float lerp(float a, float b, float t) {
    return a + ((b - a) * t);
}

[[nodiscard]] constexpr float easeOutCubic(float t) {
    const float u = 1.0F - t;
    return 1.0F - (u * u * u);
}

[[nodiscard]] constexpr float easeInCubic(float t) {
    return t * t * t;
}

[[nodiscard]] constexpr float easeOutQuad(float t) {
    return 1.0F - ((1.0F - t) * (1.0F - t));
}

// Overshoots past 1.0 near the end before settling back (a gentle "back" kick).
[[nodiscard]] constexpr float easeOutBack(float t) {
    constexpr float c1 = 1.70158F;
    constexpr float c3 = c1 + 1.0F;
    const float u = t - 1.0F;
    return 1.0F + (c3 * u * u * u) + (c1 * u * u);
}

} // namespace og::ease
