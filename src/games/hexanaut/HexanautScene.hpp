#pragma once

#include "core/Canvas.hpp"
#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/hexanaut/HexTypes.hpp"
#include "games/hexanaut/HexWorld.hpp"
#include "ui/Button.hpp"

#include <cstdint>
#include <vector>

namespace og {

class SceneManager;

// Hexanaut — a hex-grid territory game (hexanaut.io / Splix style) rendered in a
// fixed-angle fake-3D axonometric style: claimed cells and trails are extruded
// prisms drawn with flat-shaded polygons (no images). The pure simulation lives
// in HexWorld; this Scene owns the follow-camera, projects the logic plane to the
// screen, turns touch input into a hex heading, and draws the layered prism field
// plus HUD. Movement is discrete hex-stepping; the avatar is interpolated between
// cells for smooth motion.
class HexanautScene : public Scene {
public:
    HexanautScene(SceneManager& manager, Difficulty difficulty);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    enum class Phase : std::uint8_t { Playing, GameOver };

    struct ScreenPos {
        float x = 0.0F;
        float y = 0.0F;
    };

    [[nodiscard]] ScreenPos toScreen(hexanaut::Vec2 world, float lift) const;
    [[nodiscard]] hexanaut::Vec2 screenToWorld(float sx, float sy) const;
    [[nodiscard]] static hexanaut::Vec2 avatarWorld(const hexanaut::Player& p);

    bool handleBackButton(const PointerEvent& event);
    void handleSteer(const PointerEvent& event);
    void enterGameOver();
    void updateCamera(float dtSeconds);

    static void drawBackButton(Canvas& canvas);
    void appendHexTop(hexanaut::Vec2 centerWorld, float inset, float lift, Color color);
    void appendWall(hexanaut::Vec2 centerWorld, int edge, float liftTop, Color top, Color bottom);
    void appendCellPrism(const hexanaut::HexGrid& grid, hexanaut::HexCoord coord,
                         hexanaut::Vec2 center);
    void drawField(Canvas& canvas);
    void drawPowerups(Canvas& canvas) const;
    void drawAvatars(Canvas& canvas) const;
    void drawHud(Canvas& canvas) const;
    void drawLeaderboard(Canvas& canvas) const;
    void drawMinimap(Canvas& canvas);
    void drawOverlay(Canvas& canvas) const;

    struct PowerupDraw {
        hexanaut::Vec2 center;
        std::uint8_t type = 0;
    };

    SceneManager& manager_;
    Difficulty difficulty_;
    hexanaut::HexWorld world_;

    float accum_ = 0.0F;
    float camX_ = 0.0F;
    float camY_ = 0.0F;
    float zoom_ = 1.0F;

    hexanaut::Vec2 aimScreen_; // last pointer position (logical px) used to steer
    bool hasAim_ = false;
    bool backPressed_ = false;

    Phase phase_ = Phase::Playing;
    bool recorded_ = false;
    float finalPercent_ = 0.0F;
    float bestPercent_ = 0.0F;

    // Reused geometry buffers so the whole field submits in one fillMesh call.
    std::vector<Canvas::Vertex> meshVerts_;
    std::vector<int> meshIdx_;
    std::vector<PowerupDraw> powerupDraws_; // power-up cells found during drawField

    Button homeButton_;
    Button retryButton_;
};

} // namespace og
