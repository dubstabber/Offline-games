#pragma once

#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/snake/GhostLeaderboard.hpp"
#include "games/snake/SnakeTypes.hpp"
#include "games/snake/SnakeWorld.hpp"
#include "ui/IconButton.hpp"
#include "ui/ResultOverlay.hpp"

#include <cstdint>

namespace og {

class SceneManager;

// A slither.io-style arena: the player steers a growing snake toward their
// finger, eats orbs to grow, and dies on hitting another snake's body or the
// world edge — the body then becomes food. ~20 bots share the arena and a
// synthetic leaderboard makes the global rank read like the original. The scene
// owns the pure SnakeWorld sim (advanced on a fixed-timestep accumulator) and a
// camera that follows the head and zooms out as the snake grows; it renders the
// world (no images — everything is code-drawn discs + glyphs) and the HUD.
class SnakeScene : public Scene {
public:
    SnakeScene(SceneManager& manager, Difficulty difficulty);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    enum class Phase : std::uint8_t { Playing, GameOver };

    struct ScreenPos {
        float x = 0.0F;
        float y = 0.0F;
    };

    bool handleBoostButton(const PointerEvent& event);
    void handleSteer(const PointerEvent& event);
    void enterGameOver();

    [[nodiscard]] snake::Vec2 screenToWorld(float sx, float sy) const;
    [[nodiscard]] ScreenPos toScreen(snake::Vec2 world) const;
    [[nodiscard]] static bool onScreen(float sx, float sy, float screenRadius);
    void updateCamera(float dtSeconds);

    void drawArena(Canvas& canvas) const;
    void drawFood(Canvas& canvas) const;
    void drawSnake(Canvas& canvas, const snake::Snake& s, bool isPlayer) const;
    void drawHud(Canvas& canvas) const;
    void drawLeaderboard(Canvas& canvas) const;
    void drawBoostButton(Canvas& canvas) const;
    void drawOverlay(Canvas& canvas) const;

    SceneManager& manager_;
    Difficulty difficulty_;
    snake::SnakeWorld world_;
    snake::GhostLeaderboard ghost_;

    float accum_ = 0.0F;
    float camX_ = 0.0F;
    float camY_ = 0.0F;
    float zoom_ = 1.0F;

    snake::Vec2 aimScreen_; // last pointer position (logical px) used to steer
    bool hasAim_ = false;
    bool boosting_ = false;
    bool boostLatched_ = false; // the active pointer is holding the boost button
    IconButton backButton_;

    Phase phase_ = Phase::Playing;
    bool recorded_ = false;
    int finalScore_ = 0;
    int bestScore_ = 0;

    ResultOverlay overlay_;
};

} // namespace og
