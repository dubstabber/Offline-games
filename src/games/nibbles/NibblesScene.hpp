#pragma once

#include "core/Canvas.hpp"
#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/nibbles/NibblesWorld.hpp"
#include "ui/IconButton.hpp"
#include "ui/ResultOverlay.hpp"

#include <cstdint>
#include <vector>

namespace og {

class SceneManager;

[[nodiscard]] int nibblesSavedLevel(Difficulty difficulty);

class NibblesScene : public Scene {
public:
    NibblesScene(SceneManager& manager, Difficulty difficulty, int level);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;
    [[nodiscard]] bool isAnimating() const override { return phase_ == Phase::Playing; }

private:
    enum class Phase : std::uint8_t { Playing, LevelClear, GameOver };

    void layoutBoard();
    void rebuildBoardMesh();
    void snapshotBodies();
    void queueDirection(nibbles::Direction direction);
    void handleSwipe(const PointerEvent& event);
    void enterLevelClear();
    void enterGameOver();

    [[nodiscard]] float cellX(int x) const;
    [[nodiscard]] float cellY(int y) const;
    [[nodiscard]] float cellCenterX(int x) const;
    [[nodiscard]] float cellCenterY(int y) const;
    [[nodiscard]] float renderAlpha() const;

    void drawTopBar(Canvas& canvas) const;
    void drawBoard(Canvas& canvas) const;
    void drawBonuses(Canvas& canvas) const;
    void drawWorms(Canvas& canvas) const;
    void drawDpad(Canvas& canvas) const;
    void drawOverlay(Canvas& canvas) const;

    SceneManager& manager_;
    Difficulty difficulty_;
    int level_;
    nibbles::NibblesWorld world_;
    Phase phase_ = Phase::Playing;
    bool resolved_ = false;
    float accum_ = 0.0F;

    IconButton backButton_;
    IconButton upButton_;
    IconButton downButton_;
    IconButton leftButton_;
    IconButton rightButton_;
    ResultOverlay overlay_;

    float cellPx_ = 7.0F;
    float originX_ = 0.0F;
    float originY_ = 0.0F;
    std::vector<Canvas::Vertex> boardVerts_;
    std::vector<int> boardIndices_;
    std::vector<std::vector<nibbles::Position>> previousBodies_;
    bool swiping_ = false;
    float swipeStartX_ = 0.0F;
    float swipeStartY_ = 0.0F;
};

} // namespace og
