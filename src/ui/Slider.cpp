#include "ui/Slider.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"
#include "core/Theme.hpp"

#include <algorithm>
#include <cmath>

namespace og {
namespace {

// Track geometry, matching the slider in DifficultySelectScene.
constexpr float kTrackH = 28.0F;
constexpr float kKnobRadius = 34.0F;

// The knob's hit region is padded well past the thin track so it's easy to grab.
constexpr float kHitPadX = 40.0F;
constexpr float kHitPadY = 50.0F;

[[nodiscard]] float nearestStop(const std::vector<float>& stops, float v) {
    float best = stops.front();
    float bestDist = std::fabs(v - best);
    for (const float stop : stops) {
        const float dist = std::fabs(v - stop);
        if (dist < bestDist) {
            best = stop;
            bestDist = dist;
        }
    }
    return best;
}

} // namespace

Slider::Slider(float trackX, float trackCy, float trackW, float minValue, float maxValue)
    : trackX_(trackX), trackCy_(trackCy), trackW_(trackW), min_(minValue), max_(maxValue),
      value_(minValue) {}

void Slider::setValue(float value) {
    value_ = std::clamp(value, min_, max_);
    if (!stops_.empty()) {
        value_ = nearestStop(stops_, value_);
    }
}

float Slider::knobX() const {
    const float t = (max_ > min_) ? (value_ - min_) / (max_ - min_) : 0.0F;
    return trackX_ + (std::clamp(t, 0.0F, 1.0F) * trackW_);
}

void Slider::dragTo(float x) {
    const float clamped = std::clamp(x, trackX_, trackX_ + trackW_);
    const float t = (clamped - trackX_) / trackW_;
    float v = min_ + (t * (max_ - min_));
    if (!stops_.empty()) {
        v = nearestStop(stops_, v);
    }
    if (v != value_) {
        value_ = v;
        if (onChange_) {
            onChange_(value_);
        }
    }
}

bool Slider::handleInput(const PointerEvent& event) {
    const float hitX = trackX_ - kHitPadX;
    const float hitW = trackW_ + (2.0F * kHitPadX);
    const float hitY = trackCy_ - kHitPadY;
    const float hitH = 2.0F * kHitPadY;
    switch (event.phase) {
    case PointerEvent::Phase::Down:
        if (hitTest(event, hitX, hitY, hitW, hitH)) {
            dragging_ = true;
            dragTo(event.x);
            return true;
        }
        return false;
    case PointerEvent::Phase::Move:
        if (dragging_) {
            dragTo(event.x);
            return true;
        }
        return false;
    case PointerEvent::Phase::Up:
        if (dragging_) {
            dragging_ = false;
            return true;
        }
        return false;
    }
    return false;
}

void Slider::render(Canvas& canvas) const {
    const float top = trackCy_ - (kTrackH / 2.0F);
    canvas.fillRoundedRect(trackX_, top, trackW_, kTrackH, kTrackH / 2.0F, theme().sliderTrack);
    const float kx = knobX();
    canvas.fillRoundedRect(trackX_, top, kx - trackX_, kTrackH, kTrackH / 2.0F, fill_);
    canvas.fillCircle(kx, trackCy_, kKnobRadius, colors::white);
    canvas.fillCircle(kx, trackCy_, kKnobRadius - 8.0F, fill_);
}

} // namespace og
