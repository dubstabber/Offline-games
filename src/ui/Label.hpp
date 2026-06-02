#pragma once

#include "core/Canvas.hpp"
#include "core/Color.hpp"

#include <string>

namespace og {

// A static line of text/emoji at a fixed position. Trivial, but having it as a
// widget keeps scene render code declarative and consistent with Button.
class Label {
public:
    Label(std::string text, float x, float y, float pixelSize,
          Canvas::Align align = Canvas::Align::Left);

    void setText(std::string text) { text_ = std::move(text); }
    void setColor(Color color) { color_ = color; }

    void render(Canvas& canvas) const;

private:
    std::string text_;
    float x_;
    float y_;
    float pixelSize_;
    Canvas::Align align_;
    Color color_ = colors::text;
};

} // namespace og
