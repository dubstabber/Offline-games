#pragma once

#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/sokoban/SokobanBoard.hpp"
#include "ui/IconButton.hpp"
#include "ui/ResultOverlay.hpp"

#include <cstdint>

namespace og {

class SceneManager;

[[nodiscard]] int sokobanSavedLevel(Difficulty difficulty);

class SokobanScene : public Scene {
public:
    SokobanScene(SceneManager& manager, Difficulty difficulty, int level);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;
    [[nodiscard]] bool isAnimating() const override { return false; }

private:
    enum class Phase : std::uint8_t { Playing, Solved };

    void layoutBoard();
    [[nodiscard]] bool cellAt(float px, float py, int& x, int& y) const;
    [[nodiscard]] float cellX(int x) const;
    [[nodiscard]] float cellY(int y) const;
    [[nodiscard]] float cellCenterX(int x) const;
    [[nodiscard]] float cellCenterY(int y) const;

    void tryMove(SokobanBoard::Direction direction);
    void moveFromBoardTap(float px, float py);
    void onSolved();

    void drawTopBar(Canvas& canvas) const;
    void drawBoard(Canvas& canvas) const;
    void drawGoals(Canvas& canvas) const;
    void drawBoxes(Canvas& canvas) const;
    void drawPlayer(Canvas& canvas) const;
    void drawDpad(Canvas& canvas) const;
    void drawOverlay(Canvas& canvas) const;

    SceneManager& manager_;
    Difficulty difficulty_;
    int level_;
    SokobanBoard board_;
    Phase phase_ = Phase::Playing;
    int sourceSet_ = 0;
    int sourceLevel_ = 1;

    IconButton backButton_;
    IconButton undoButton_;
    IconButton resetButton_;
    IconButton upButton_;
    IconButton downButton_;
    IconButton leftButton_;
    IconButton rightButton_;

    float cellPx_ = 48.0F;
    float originX_ = 0.0F;
    float originY_ = 0.0F;

    ResultOverlay overlay_;
};

} // namespace og
