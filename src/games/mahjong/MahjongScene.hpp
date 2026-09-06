#pragma once

#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/mahjong/MahjongBoard.hpp"
#include "ui/IconButton.hpp"
#include "ui/ResultOverlay.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace og {

class Canvas;
class SceneManager;
using Color = SDL_Color;

// The current (1-based) Mahjong level for a difficulty, read from Settings.
// Exposed so the registry's create / currentLevel can launch and label the
// saved level (mirrors arrowsSavedLevel).
[[nodiscard]] int mahjongSavedLevel(Difficulty difficulty);

// Renders a Mahjong solitaire board: ivory tiles with a tan side, stacked so
// each layer lifts up and to the left, faces drawn from code (dots, bamboo,
// numbers, wind and dragon letters) or emoji (flowers, seasons, the red
// dragon). Tap a free tile to select it and a matching free tile to clear the
// pair; blocked tiles wobble. When no pair is left the remaining tiles are
// re-dealt solvably after a short banner. Clearing the board advances the
// saved level and offers NEXT. Boards come from MahjongLayouts and are dealt
// solvable, identical on every retry.
class MahjongScene : public Scene {
public:
    MahjongScene(SceneManager& manager, Difficulty difficulty, int level);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;
    [[nodiscard]] bool isAnimating() const override;

private:
    enum class Phase : std::uint8_t { Playing, Won };

    struct Rect {
        float x = 0.0F;
        float y = 0.0F;
        float w = 0.0F;
        float h = 0.0F;
    };

    // A cleared tile on its way out: swells, then shrinks away.
    struct Vanish {
        int id = -1;
        float t = 0.0F;
    };

    void tapAt(float px, float py);
    [[nodiscard]] int tileAt(float px, float py) const;
    void finishIfOver();

    void layoutBoard();
    [[nodiscard]] Rect faceRect(const MahjongBoard::Tile& tile) const;

    void drawTopBar(Canvas& canvas) const;
    void drawTiles(Canvas& canvas) const;
    void drawTile(Canvas& canvas, const MahjongBoard::Tile& tile, Rect r, float scale,
                  Color face) const;
    void drawFace(Canvas& canvas, const MahjongBoard::Tile& tile, float bounce) const;
    void drawVanishes(Canvas& canvas) const;
    void drawOverlay(Canvas& canvas) const;

    SceneManager& manager_;
    Difficulty difficulty_;
    int level_;
    std::string layoutName_;
    MahjongBoard board_;
    Phase phase_ = Phase::Playing;
    IconButton backButton_;
    ResultOverlay overlay_;

    std::vector<int> drawOrder_; // tile ids by layer, then row, then column
    std::vector<Vanish> vanishes_;
    int shakeId_ = -1;
    float shakeT_ = 1.0F;
    float shuffleT_ = 1.0F; // bounce after a shuffle
    // Set when no pair can be matched; after a short banner the remaining
    // tiles are re-dealt (solvably) by themselves.
    bool noMoves_ = false;
    float noMovesT_ = 0.0F;

    // Board placement in pixels, recomputed per layout so each size fits.
    float tileW_ = 60.0F;
    float tileH_ = 80.0F;
    float lift_ = 7.0F; // how far each layer rises up-left
    float originX_ = 0.0F;
    float originY_ = 0.0F;
};

} // namespace og
