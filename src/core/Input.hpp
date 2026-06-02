#pragma once

#include <cstdint>

namespace og {

// A pointer interaction expressed in logical canvas coordinates (see Layout).
// Touch (finger) and mouse are unified into the same event by App before a
// scene ever sees them, so games never branch on the input device.
struct PointerEvent {
    enum class Phase : std::uint8_t { Down, Up };

    Phase phase{};
    float x = 0.0F; // logical pixels
    float y = 0.0F; // logical pixels
};

// Hit-test helper: is the pointer inside the given logical-pixel rectangle?
[[nodiscard]] constexpr bool hitTest(const PointerEvent& p, float x, float y, float w, float h) {
    return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
}

} // namespace og
