#include "ui/Toggle.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"

namespace og {
namespace {

constexpr Color kOffTrack = rgb(176, 178, 186); // neutral grey when off
constexpr Color kOffMark = rgb(120, 122, 130);  // the × cue when off

} // namespace

Toggle::Toggle(float x, float y, float w, float h) : x_(x), y_(y), w_(w), h_(h) {}

bool Toggle::handleInput(const PointerEvent& event) {
    if (event.phase == PointerEvent::Phase::Move) {
        return false; // a toggle only cares about press and release
    }
    const bool inside = hitTest(event, x_, y_, w_, h_);
    if (event.phase == PointerEvent::Phase::Down) {
        pressed_ = inside;
        return inside;
    }
    // Phase::Up — flip only if the tap both started and ended on us.
    const bool wasPressed = pressed_;
    pressed_ = false;
    if (wasPressed && inside) {
        on_ = !on_;
        if (onChange_) {
            onChange_(on_);
        }
        return true;
    }
    return false;
}

void Toggle::render(Canvas& canvas) const {
    const float r = h_ / 2.0F;
    canvas.fillRoundedRect(x_, y_, w_, h_, r, on_ ? colors::menuPurple : kOffTrack);

    const float knobR = r - (h_ * 0.12F);
    const float cy = y_ + r;
    const float knobCx = on_ ? (x_ + w_ - r) : (x_ + r);
    // Draw the cue on the side opposite the knob: ✓ (left) when on, × (right) off.
    const float markCx = on_ ? (x_ + r) : (x_ + w_ - r);
    const float m = knobR * 0.5F;
    if (on_) {
        canvas.line(markCx - m, cy, markCx - (m * 0.2F), cy + m, 7.0F, colors::white);
        canvas.line(markCx - (m * 0.2F), cy + m, markCx + m, cy - m, 7.0F, colors::white);
    } else {
        canvas.line(markCx - m, cy - m, markCx + m, cy + m, 7.0F, kOffMark);
        canvas.line(markCx - m, cy + m, markCx + m, cy - m, 7.0F, kOffMark);
    }
    canvas.fillCircle(knobCx, cy, knobR, colors::white);
}

} // namespace og
