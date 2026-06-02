#pragma once

#include "core/Color.hpp"

#include <functional>
#include <string>

namespace og {

class Canvas;
struct PointerEvent;

// A touch-friendly, code-drawn button: a rounded rectangle with a centered
// label (which may contain emoji). It tracks its own pressed state for visual
// feedback and fires onTap when a press-and-release lands inside its bounds.
class Button {
public:
    Button(std::string label, float x, float y, float w, float h);

    void setLabel(std::string label) { label_ = std::move(label); }
    void setColors(Color fill, Color text);
    void setOnTap(std::function<void()> onTap) { onTap_ = std::move(onTap); }

    // Returns true if the event was consumed by this button.
    bool handleInput(const PointerEvent& event);
    void render(Canvas& canvas) const;

private:
    std::string label_;
    float x_;
    float y_;
    float w_;
    float h_;
    Color fill_ = colors::surfaceAlt;
    Color textColor_ = colors::text;
    bool pressed_ = false;
    std::function<void()> onTap_;
};

} // namespace og
