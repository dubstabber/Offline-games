#include "ui/IconButton.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"
#include "core/Theme.hpp"

#include <utility>

namespace og {
namespace {

constexpr const char* kBackGlyph = "\xE2\xAC\x85"; // ⬅ (color emoji via the emoji font)
constexpr float kBackGlyphScale = 0.95F;           // glyph height as a fraction of the radius

} // namespace

IconButton::IconButton(Icon icon, float cx, float cy, float radius)
    : icon_(icon), cx_(cx), cy_(cy), radius_(radius) {}

void IconButton::setGlyph(std::string glyph, float glyphSize) {
    glyph_ = std::move(glyph);
    glyphSize_ = glyphSize;
}

bool IconButton::handleInput(const PointerEvent& event) {
    if (event.phase == PointerEvent::Phase::Move) {
        return false; // a tap button only cares about press and release
    }
    // Square touch box around the disc — a comfortable target slightly larger
    // than the circle, matching the per-scene back/reset hit tests this replaces.
    const bool inside =
        hitTest(event, cx_ - radius_, cy_ - radius_, radius_ * 2.0F, radius_ * 2.0F);
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

void IconButton::render(Canvas& canvas) const {
    canvas.fillCircle(cx_, cy_, radius_, theme().backCircle);
    if (icon_ == Icon::Back) {
        canvas.emojiCentered(kBackGlyph, cx_, cy_, radius_ * kBackGlyphScale);
    } else {
        canvas.emojiCentered(glyph_, cx_, cy_, glyphSize_);
    }
}

} // namespace og
