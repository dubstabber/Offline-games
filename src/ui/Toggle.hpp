#pragma once

#include "core/Color.hpp"

#include <functional>

namespace og {

class Canvas;
struct PointerEvent;

// A pill switch: a rounded track with a circular knob that slides left (off) to
// right (on). Grey when off, purple when on, with a small ×/✓ cue — like a
// standard mobile toggle. Tracks its own pressed state and fires onChange when a
// press-and-release lands inside it. Self-contained, so it looks the same in
// both themes (it's a control, not chrome).
class Toggle {
public:
    Toggle(float x, float y, float w, float h);

    void setValue(bool on) { on_ = on; }
    [[nodiscard]] bool value() const { return on_; }
    void setOnChange(std::function<void(bool)> onChange) { onChange_ = std::move(onChange); }

    // Returns true if the event was consumed by this toggle.
    bool handleInput(const PointerEvent& event);
    void render(Canvas& canvas) const;

private:
    float x_;
    float y_;
    float w_;
    float h_;
    bool on_ = false;
    bool pressed_ = false;
    std::function<void(bool)> onChange_;
};

} // namespace og
