#include "ui/Label.hpp"

namespace og {

Label::Label(std::string text, float x, float y, float pixelSize, Canvas::Align align)
    : text_(std::move(text)), x_(x), y_(y), pixelSize_(pixelSize), align_(align) {}

void Label::render(Canvas& canvas) const {
    canvas.text(text_, x_, y_, pixelSize_, color_, align_);
}

} // namespace og
