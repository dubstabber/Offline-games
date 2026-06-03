#pragma once

#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/tapmatch/TapMatchBoard.hpp"
#include "ui/Button.hpp"

#include <cstdint>
#include <string>

namespace og {

class SceneManager;

// Renders a Tap Match board — overlapping piles of fruit-emoji tiles on a maroon
// field with a 7-slot holder bar — and turns taps into moves. Tapping an
// uncovered (bright) tile sends its fruit to the holder; three of a kind clear.
// Clear the board to win; fill the holder with no triple to lose. Difficulty
// scales fruit variety, tile count, and how deep the piles stack.
class TapMatchScene : public Scene {
public:
    TapMatchScene(SceneManager& manager, Difficulty difficulty);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    // Playing: accept taps. GameOver: the result overlay is shown and only its
    // buttons respond.
    enum class Phase : std::uint8_t { Playing, GameOver };

    bool handleBackButton(const PointerEvent& event);
    void beginRound();
    void enterGameOver();

    // The id of the uncovered tile under (px, py), or -1 if the topmost tile
    // there is covered or there is none.
    [[nodiscard]] int pickAccessibleAt(float px, float py) const;
    [[nodiscard]] std::string statusText() const;
    [[nodiscard]] const char* resultText() const;

    static void drawBackButton(Canvas& canvas);
    void drawTopBar(Canvas& canvas) const;
    void drawBoard(Canvas& canvas) const;
    void drawHolder(Canvas& canvas) const;
    void drawOverlay(Canvas& canvas) const;

    SceneManager& manager_;
    Difficulty difficulty_;
    TapMatchBoard board_;
    Phase phase_ = Phase::Playing;
    bool backPressed_ = false;
    Button homeButton_;
    Button playAgainButton_;
};

} // namespace og
