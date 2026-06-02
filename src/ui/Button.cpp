#include "ui/Button.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"

namespace og {

Button::Button(std::string label, float x, float y, float w, float h)
    : label_(std::move(label)), x_(x), y_(y), w_(w), h_(h) {}

void Button::setColors(Color fill, Color text) {
    fill_ = fill;
    textColor_ = text;
}

bool Button::handleInput(const PointerEvent& event) {
    const bool inside = hitTest(event, x_, y_, w_, h_);
    if (event.phase == PointerEvent::Phase::Down) {
        pressed_ = inside;
        return inside;
    }
    // Phase::Up — a tap completes only if it both started and ended on us.
    const bool wasPressed = pressed_;
    pressed_ = false;
    if (wasPressed && inside) {
        if (onTap_) {
            onTap_();
        }
        return true;
    }
    return false;
}

void Button::render(Canvas& canvas) const {
    const Color fill = pressed_ ? colors::accent : fill_;
    const float radius = h_ * 0.22F;
    canvas.fillRoundedRect(x_, y_, w_, h_, radius, fill);
    canvas.textCentered(label_, x_ + (w_ / 2.0F), y_ + (h_ / 2.0F), h_ * 0.42F, textColor_);
}

} // namespace og
