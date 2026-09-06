#pragma once

#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/twentyfortyeight/TwentyFortyEightBoard.hpp"
#include "ui/IconButton.hpp"
#include "ui/ResultOverlay.hpp"

#include <cstdint>

namespace og {

class Canvas;
class SceneManager;

// 2048: swipe to slide every tile; equal tiles that collide merge into their sum.
// Difficulty picks the board size (5x5 / 4x4 / 3x3) and the goal tile. The pure
// rules live in TwentyFortyEightBoard; this Scene turns swipes into moves,
// animates each move (a slide, then the merged tiles pop and the new tile grows
// in), offers one-step undo and restart, and persists the best score per
// difficulty. Reaching the goal shows a win overlay once; KEEP GOING resumes an
// endless game.
class TwentyFortyEightScene : public Scene {
public:
    TwentyFortyEightScene(SceneManager& manager, Difficulty difficulty);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;
    // Only a move's slide/pop animation needs continuous redraws; the board is
    // otherwise static, so the app can idle between swipes.
    [[nodiscard]] bool isAnimating() const override;

private:
    using Board = TwentyFortyEightBoard;
    // Won: the goal tile was just reached. GameOver: no move can change the board.
    enum class Phase : std::uint8_t { Playing, Won, GameOver };

    // One move's animation: tiles glide for kSlideSeconds, then the merged tiles
    // bulge and the spawned tile grows in over kPopSeconds.
    static constexpr float kSlideSeconds = 0.11F;
    static constexpr float kPopSeconds = 0.15F;
    static constexpr float kAnimTotal = kSlideSeconds + kPopSeconds;

    void handleSwipe(const PointerEvent& event);
    void applyMove(Board::Direction dir);
    void undoMove();
    void restart();
    void finishAnimation();
    void saveBest();

    [[nodiscard]] float cellX(int x) const;
    [[nodiscard]] float cellY(int y) const;
    void drawTopBar(Canvas& canvas) const;
    void drawScores(Canvas& canvas) const;
    void drawBoard(Canvas& canvas) const;
    void drawSlidingTiles(Canvas& canvas) const;
    void drawSettledTiles(Canvas& canvas) const;
    // Scale of the settled tile at (x, y) `u` (0..1) into the pop phase: merged
    // results bulge, the spawned tile grows in, everything else is 1.
    [[nodiscard]] float settledScale(int x, int y, float u) const;
    static void drawTile(Canvas& canvas, float cx, float cy, float size, int value);
    void drawOverlay(Canvas& canvas) const;

    SceneManager& manager_;
    Difficulty difficulty_;
    TwentyFortyEightParams params_;
    Board board_;
    Phase phase_ = Phase::Playing;
    Phase pendingPhase_ = Phase::Playing; // overlay to show once the move's animation ends
    bool goalReached_ = false;            // the win overlay shows once per game
    int best_;
    bool bestDirty_ = false; // best_ is ahead of the saved settings

    bool swiping_ = false; // a press is down and has not yet produced a move
    float swipeX_ = 0.0F;
    float swipeY_ = 0.0F;

    Board::MoveResult lastMove_;
    float animT_ = kAnimTotal; // seconds into the last move's animation (>= total when idle)

    float cellPx_ = 0.0F;

    IconButton backButton_;
    IconButton undoButton_;
    IconButton restartButton_;
    ResultOverlay overlay_;
};

} // namespace og
