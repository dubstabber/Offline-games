#pragma once

#include <cstdint>

namespace og {

// A pointer interaction expressed in logical canvas coordinates (see Layout).
// Touch (finger) and mouse are unified into the same event by App before a
// scene ever sees them, so games never branch on the input device. A press is
// a Down, then zero or more Moves while the finger/mouse drags, then an Up.
struct PointerEvent {
    enum class Phase : std::uint8_t { Down, Move, Up };

    Phase phase{};
    float x = 0.0F; // logical pixels
    float y = 0.0F; // logical pixels
};

// Hit-test helper: is the pointer inside the given logical-pixel rectangle?
[[nodiscard]] constexpr bool hitTest(const PointerEvent& p, float x, float y, float w, float h) {
    return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
}

} // namespace og
