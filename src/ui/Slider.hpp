#pragma once

#include "core/Color.hpp"

#include <functional>
#include <vector>

namespace og {

class Canvas;
struct PointerEvent;

// A horizontal slider: a rounded track with a coloured fill up to a draggable
// knob. Either continuous over [min, max], or snapped to a set of discrete stops
// (setStops). Tap anywhere on the track or drag the knob to set the value; fires
// onChange whenever the value changes. The unfilled track follows the theme; the
// fill/knob use a settable accent. Generalizes the slider in DifficultySelectScene.
class Slider {
public:
    Slider(float trackX, float trackCy, float trackW, float minValue, float maxValue);

    void setStops(std::vector<float> stops) { stops_ = std::move(stops); }
    void setValue(float value); // clamped to [min, max], snapped to stops if any
    [[nodiscard]] float value() const { return value_; }
    void setFill(Color fill) { fill_ = fill; }
    void setOnChange(std::function<void(float)> onChange) { onChange_ = std::move(onChange); }

    // Returns true if the event was consumed by this slider.
    bool handleInput(const PointerEvent& event);
    void render(Canvas& canvas) const;

private:
    [[nodiscard]] float knobX() const; // x of the knob centre for the current value
    void dragTo(float x);              // move to the value under x, snapping to stops

    float trackX_;
    float trackCy_;
    float trackW_;
    float min_;
    float max_;
    std::vector<float> stops_; // empty = continuous
    float value_;
    Color fill_ = colors::menuPurple;
    bool dragging_ = false;
    std::function<void(float)> onChange_;
};

} // namespace og
