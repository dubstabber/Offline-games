#include "ui/ResultOverlay.hpp"

#include "core/Canvas.hpp"
#include "core/Layout.hpp"

#include <utility>

namespace og {
namespace {

// The button row layout shared by every game's result screen: a square Home
// button, a gap, then a wider action button, the whole row centered.
constexpr float kHomeSize = 140.0F;
constexpr float kButtonGap = 24.0F;
constexpr float kActionW = 360.0F;
constexpr float kRowWidth = kHomeSize + kButtonGap + kActionW;
constexpr float kRowX = (layout::kWidthF - kRowWidth) / 2.0F;
constexpr const char* kHome = "\xF0\x9F\x8F\xA0"; // 🏠

} // namespace

ResultOverlay::ResultOverlay(Color actionFill, Color actionText, float rowY)
    : homeButton_(kHome, kRowX, rowY, kHomeSize, kHomeSize),
      actionButton_("", kRowX + kHomeSize + kButtonGap, rowY, kActionW, kHomeSize) {
    homeButton_.setColors(colors::white, colors::panelBrown);
    actionButton_.setColors(actionFill, actionText);
}

void ResultOverlay::setOnHome(std::function<void()> onHome) {
    homeButton_.setOnTap(std::move(onHome));
}

void ResultOverlay::setOnAction(std::function<void()> onAction) {
    actionButton_.setOnTap(std::move(onAction));
}

void ResultOverlay::setActionLabel(std::string label) {
    actionButton_.setLabel(std::move(label));
}

bool ResultOverlay::handleInput(const PointerEvent& event) {
    if (homeButton_.handleInput(event)) {
        return true;
    }
    return actionButton_.handleInput(event);
}

void ResultOverlay::render(Canvas& canvas, std::string_view title, float titleCy,
                           float titleSize) const {
    canvas.fillRect(0.0F, 0.0F, layout::kWidthF, layout::kHeightF, colors::overlay);
    canvas.textCentered(title, layout::kWidthF / 2.0F, titleCy, titleSize, colors::white);
    homeButton_.render(canvas);
    actionButton_.render(canvas);
}

} // namespace og
