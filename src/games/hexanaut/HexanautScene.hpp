#pragma once

#include "core/Canvas.hpp"
#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/hexanaut/HexTypes.hpp"
#include "games/hexanaut/HexWorld.hpp"
#include "ui/Button.hpp"

#include <cstdint>
#include <random>
#include <vector>

namespace og {

class SceneManager;

// Hexanaut — a hex-grid territory game (hexanaut.io / Splix style) rendered in a
// fixed-angle fake-3D axonometric style: claimed cells and trails are extruded
// prisms drawn with flat-shaded polygons (no images). The pure simulation lives
// in HexWorld; this Scene owns the follow-camera, projects the logic plane to the
// screen, turns touch input into a steering angle, and draws the layered prism
// field plus HUD. Movement is free: the avatar holds a continuous world position
// and heading that curves toward the finger, claiming the hex cells it crosses.
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
    // Append a hex top face. The face is gradient-shaded from a lit north edge
    // (colorTop) down to a darker south edge (colorBottom) for a bevelled look;
    // pass the same color twice for a flat fill.
    void appendHexTop(hexanaut::Vec2 centerWorld, float inset, float lift, Color colorTop,
                      Color colorBottom);
    void appendWall(hexanaut::Vec2 centerWorld, int edge, float liftTop, Color top, Color bottom);
    // Extruded territory block (front-facing walls + lit top face) for `owner`.
    // Shared by owned cells and by cells a rival is cutting a trail across, so the
    // enemy's block keeps its fill/height under the attacker's outline.
    void appendOwnedPrism(const hexanaut::HexGrid& grid, hexanaut::HexCoord coord,
                          hexanaut::Vec2 center, hexanaut::PlayerId owner);
    void appendCellPrism(const hexanaut::HexGrid& grid, hexanaut::HexCoord coord,
                         hexanaut::Vec2 center);
    void drawField(Canvas& canvas);
    void drawTrailOutlines(Canvas& canvas) const;
    void drawTrails(Canvas& canvas);
    // Spark FX kicked up while an avatar cuts across another player's territory.
    // Spawn rate is dt-accumulated (spawnCutFx); updateParticles advances + culls;
    // drawParticles batches the live sparks into one mesh. Pure cosmetics — no
    // simulation state, so it never touches HexWorld.
    void spawnCutFx(float dtSeconds);
    void updateParticles(float dtSeconds);
    void drawParticles(Canvas& canvas);
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

    // An active-trail hex collected during drawField and stroked as a bright
    // owner-colored outline in drawTrailOutlines (the out-of-territory look). `lift`
    // is the surface it sits on: 0 over flat ground, the prism top when the trail
    // crosses (and is drawn over) another player's raised territory block.
    struct TrailOutline {
        hexanaut::Vec2 center;
        hexanaut::PlayerId owner = 0;
        float lift = 0.0F;
    };

    // A single debris spark thrown off while cutting through enemy land. Lives in
    // world space (pos + a vertical "lift" that arcs up and falls under gravity)
    // so it projects through the same camera as everything else and fades by age.
    struct Particle {
        hexanaut::Vec2 pos;
        hexanaut::Vec2 vel;
        float lift = 0.0F;
        float liftVel = 0.0F;
        float age = 0.0F;
        float life = 0.4F;
        float size = 4.0F;
        Color color{};
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
    std::vector<PowerupDraw> powerupDraws_;   // power-up cells found during drawField
    std::vector<TrailOutline> trailOutlines_; // active-trail cells found during drawField
    std::vector<ScreenPos> ropeScratch_;      // reused per-player rope path in drawTrails

    std::vector<Particle> particles_; // live cut-through sparks
    float fxSpawnAccum_ = 0.0F;       // dt accumulator pacing spark bursts
    std::mt19937 fxRng_;              // visual-only jitter (fixed seed; not gameplay)

    Button homeButton_;
    Button retryButton_;
};

} // namespace og
