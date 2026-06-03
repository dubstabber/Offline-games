#pragma once

#include "core/Scene.hpp"
#include "games/GameInfo.hpp"

#include <vector>

namespace og {

class SceneManager;

// The home screen: a fixed top bar (a placeholder menu button) above a
// drag-scrollable 2-column grid of game cards built from gameRegistry().
// Tapping a card opens that game's difficulty screen; placeholder cards (no
// `create` factory) are marked "SOON" and ignore taps.
class MenuScene : public Scene {
public:
    explicit MenuScene(SceneManager& manager);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    struct Card {
        float x = 0.0F; // content-space top-left; subtract scrollY_ to draw/hit-test
        float y = 0.0F;
        float w = 0.0F;
        float h = 0.0F;
        GameInfo info;
    };

    // Index of the card at the pointer's current screen position, or -1.
    [[nodiscard]] int cardAt(const PointerEvent& event) const;
    void openCard(const Card& card);

    SceneManager& manager_;
    std::vector<Card> cards_;
    float scrollY_ = 0.0F;           // how far the grid is scrolled up, in logical pixels
    float maxScroll_ = 0.0F;         // upper bound for scrollY_ (computed from card count)
    float velocityY_ = 0.0F;         // fling speed in px/s (sign matches scrollY_), 0 when idle
    float dragAccumY_ = 0.0F;        // scrollY_ change applied by drags this frame, for velocity
    int pressedIndex_ = -1;          // card being pressed (for visual feedback), or -1
    bool gestureActive_ = false;     // a press that began in the scrollable area
    bool gestureScrolled_ = false;   // the press moved far enough to be a scroll
    bool menuButtonPressed_ = false; // the hamburger (Settings) button is pressed
    float pressStartY_ = 0.0F;       // pointer y at press, to tell taps from scrolls
    float lastPointerY_ = 0.0F;      // pointer y of the previous move, for the delta
};

} // namespace og
