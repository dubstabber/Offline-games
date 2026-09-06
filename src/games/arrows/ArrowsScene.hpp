#pragma once

#include "core/Scene.hpp"
#include "games/arrows/ArrowsBoard.hpp"
#include "games/Difficulty.hpp"
#include "ui/IconButton.hpp"
#include "ui/ResultOverlay.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace og {

class Canvas;
class SceneManager;
using Color = SDL_Color;

// The current (1-based) Arrows level for a difficulty, read from Settings.
// Exposed so the registry's create / currentLevel can launch and label the
// saved level (mirrors blockFillSavedLevel).
[[nodiscard]] int arrowsSavedLevel(Difficulty difficulty);

// Renders an Arrows board — ink polylines with chevron heads on a dotted paper
// field — and turns taps into moves. A tapped arrow that is free slides off
// the board along its own path like a train; a blocked one bumps into the
// arrow in its way, flashes red together with it, and costs a heart. Three
// spent hearts lose the level (RETRY); clearing every arrow wins it, advances
// the saved level, and offers NEXT. Boards come from ArrowsGenerator, so every
// level is solvable and identical on every retry.
class ArrowsScene : public Scene {
public:
    ArrowsScene(SceneManager& manager, Difficulty difficulty, int level);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;
    [[nodiscard]] bool isAnimating() const override;

private:
    enum class Phase : std::uint8_t { Playing, Won, Lost };

    struct Pt {
        float x = 0.0F;
        float y = 0.0F;
    };

    // An arrow sliding off the board: the window [s, s + length] of its own
    // trajectory (tail extension, cell centres, then straight out past the
    // edge) advances until the tail has left the board at `end`.
    struct Flight {
        std::vector<Pt> path; // trajectory in pixels
        float length = 0.0F;  // drawn arc length from tail cap to head tip
        float s = 0.0F;       // how far along the trajectory the tail is
        float end = 0.0F;     // s at which the arrow is fully off the board
    };

    // A blocked tap: the arrow nudges forward into the blocker and back while
    // both flash red.
    struct Shake {
        int arrow = -1;
        int blocker = -1;
        float t = 0.0F;
    };

    void tapAt(float px, float py);
    [[nodiscard]] int arrowNear(float px, float py) const;
    void restart();
    void finishIfOver(); // move to Won / Lost once the animations settle

    void layoutBoard();
    [[nodiscard]] Pt cellCenter(ArrowCell cell) const;
    [[nodiscard]] float drawnLength(const Arrow& arrow) const;
    [[nodiscard]] std::vector<Pt> trajectory(const Arrow& arrow) const;
    [[nodiscard]] Flight makeFlight(const Arrow& arrow) const;
    // The shake offset (pixels forward along the path) and colour of a live arrow.
    [[nodiscard]] float shakeOffset(int arrow) const;
    [[nodiscard]] Color arrowColor(int arrow) const;

    void drawTopBar(Canvas& canvas) const;
    void drawHearts(Canvas& canvas) const;
    void drawDots(Canvas& canvas) const;
    void drawArrows(Canvas& canvas) const;
    void drawFlights(Canvas& canvas) const;
    // Draw the part of `path` between arc lengths s0 and s1 as an arrow: round
    // tail cap, shaft with rounded corners, chevron head at s1. Clipped to the
    // board so a departing arrow never crosses the chrome.
    void drawArrowWindow(Canvas& canvas, const std::vector<Pt>& path, float s0, float s1,
                         Color color) const;
    void drawOverlay(Canvas& canvas) const;

    SceneManager& manager_;
    Difficulty difficulty_;
    int level_;
    ArrowsBoard board_;
    Phase phase_ = Phase::Playing;
    IconButton backButton_;
    IconButton resetButton_;
    ResultOverlay overlay_;

    std::vector<Flight> flights_;
    std::vector<Shake> shakes_;
    // Per heart: seconds since it was spent (negative = still alive), driving
    // the pop when it goes.
    std::array<float, ArrowsBoard::kHearts> heartLost_{};

    // Board placement in pixels, recomputed per board so each size fits.
    float cellPx_ = 60.0F;
    float originX_ = 0.0F;
    float originY_ = 0.0F;
};

} // namespace og
