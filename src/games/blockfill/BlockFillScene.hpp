#pragma once

#include "core/Scene.hpp"
#include "games/blockfill/BlockFillBoard.hpp"
#include "games/Difficulty.hpp"
#include "ui/Button.hpp"

#include <cstdint>

namespace og {

class SceneManager;

// The current (1-based) Block Fill level for a difficulty, read from Settings.
// Exposed so the registry's create / currentLevel can launch and label the saved
// level (mirrors tapMatchSavedLevel).
[[nodiscard]] int blockFillSavedLevel(Difficulty difficulty);

// Renders a Block Fill board — gray rounded cells on a dark field, a single
// light-blue rope with a darker tube through the cell centres and a round head —
// and turns a drag into rope moves. Dragging from the rope extends it onto fresh
// adjacent cells; dragging back retracts it. Clearing every playable cell solves
// the level, advances the saved level, and offers NEXT. Boards are the original
// game's, split into four difficulty pools (see BlockFillLevels).
class BlockFillScene : public Scene {
public:
    BlockFillScene(SceneManager& manager, Difficulty difficulty, int level);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    // Playing: a drag traces the rope. Solved: only the result overlay responds.
    enum class Phase : std::uint8_t { Playing, Solved };

    bool handleBackButton(const PointerEvent& event);
    bool handleResetButton(const PointerEvent& event);
    void handleDrag(const PointerEvent& event);
    void onSolved(); // persist + advance, show the overlay

    void layoutBoard(); // fit the grid into the play area and centre it
    // Map a pixel to a playable cell; false if outside the grid or on a hole.
    [[nodiscard]] bool cellAt(float px, float py, int& x, int& y) const;
    [[nodiscard]] float cellCenterX(int x) const;
    [[nodiscard]] float cellCenterY(int y) const;

    static void drawBackButton(Canvas& canvas);
    static void drawResetButton(Canvas& canvas);
    void drawTopBar(Canvas& canvas) const;
    void drawGrid(Canvas& canvas) const;
    void drawRope(Canvas& canvas) const;
    void drawOverlay(Canvas& canvas) const;

    SceneManager& manager_;
    Difficulty difficulty_;
    int level_;
    BlockFillBoard board_;
    Phase phase_ = Phase::Playing;
    bool backPressed_ = false;
    bool resetPressed_ = false;
    bool dragging_ = false;
    int lastCellX_ = -1; // last grid cell the drag processed (avoids re-stepping)
    int lastCellY_ = -1;

    // Board placement in pixels, recomputed per board so each size fits.
    float cellPx_ = 80.0F;
    float originX_ = 0.0F;
    float originY_ = 0.0F;

    Button homeButton_;
    Button nextButton_;
};

} // namespace og
