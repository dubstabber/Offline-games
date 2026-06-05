#pragma once

#include "core/Color.hpp"
#include "ui/Button.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace og {

class Canvas;
struct PointerEvent;

// The shared game-over / result panel every game shows: a translucent full-screen
// dim, a big centered title, and a row of a square Home button beside a wider
// action button (PLAY AGAIN / NEXT / RETRY / REPLAY). It owns the two ui::Buttons
// and the standard centered row layout (Home 140, gap 24, action 360) so each
// game stops re-deriving it. Home is always the house white-on-panelBrown 🏠; the
// scene wires its handlers and sets the action label/color. Any score/subtitle
// lines are game-specific, so the scene draws those itself (they sit in the empty
// band between the title and the button row, so render order doesn't matter).
class ResultOverlay {
public:
    // `actionFill`/`actionText` color the action button. `rowY` is the button row
    // top: 760 for the shorter-board games (Tic-Tac-Toe, Tap Match), 820 default.
    ResultOverlay(Color actionFill, Color actionText, float rowY = 820.0F);

    void setOnHome(std::function<void()> onHome);
    void setOnAction(std::function<void()> onAction);
    void setActionLabel(std::string label);

    // Returns true if the event was consumed (Home or action button).
    bool handleInput(const PointerEvent& event);

    // Dim + centered title at (titleCy, titleSize) + the Home and action buttons.
    void render(Canvas& canvas, std::string_view title, float titleCy, float titleSize) const;

private:
    Button homeButton_;
    Button actionButton_;
};

} // namespace og
