#pragma once

#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/minesweeper/MineSweeperBoard.hpp"
#include "ui/IconButton.hpp"
#include "ui/ResultOverlay.hpp"

#include <cstdint>

namespace og {

class SceneManager;

// Renders a Minesweeper board — covered tiles on a dark slate field, classic
// number colours, mine/flag emoji — and turns taps into moves. A shovel/flag
// toggle switches between revealing and flagging (touch has no right-click);
// tapping a satisfied number chords. Boards are generated solvable by logic on
// the first tap (see MineSweeperBoard / MineSweeperSolver). The HUD shows the
// current and all-time win streak (persisted per difficulty) and the mines-left
// counter, mirroring the original's screen.
class MineSweeperScene : public Scene {
public:
    MineSweeperScene(SceneManager& manager, Difficulty difficulty);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    // Playing: taps reveal/flag/chord. GameOver: only the result overlay responds.
    enum class Phase : std::uint8_t { Playing, GameOver };

    bool handleModeToggle(const PointerEvent& event);
    void handleBoardTap(const PointerEvent& event);

    void beginRound();    // fresh board, same difficulty (does not touch the streak)
    void enterGameOver(); // record + persist the streak, show the overlay
    void layoutBoard();   // fit the grid into the play area and centre it
    // Map a pixel to a cell; false if outside the grid.
    [[nodiscard]] bool cellAt(float px, float py, int& row, int& col) const;
    [[nodiscard]] const char* resultText() const;

    void drawTopBar(Canvas& canvas) const;
    void drawBoard(Canvas& canvas) const;
    void drawBottomBar(Canvas& canvas) const;
    void drawOverlay(Canvas& canvas) const;

    SceneManager& manager_;
    Difficulty difficulty_;
    MineSweeperBoard board_;
    Phase phase_ = Phase::Playing;
    bool flagging_ = false; // false = dig/reveal mode, true = flag mode
    IconButton backButton_;
    IconButton resetButton_;
    bool recorded_ = false; // streak already recorded for this game-over
    int currentStreak_ = 0;
    int bestStreak_ = 0;

    // Board placement in pixels, recomputed per board so each difficulty fits.
    float cellPx_ = 64.0F;
    float originX_ = 0.0F;
    float originY_ = 0.0F;

    ResultOverlay overlay_;
};

} // namespace og
