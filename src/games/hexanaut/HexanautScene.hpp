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
    // Spawn one fading laser bolt per new shooter capture (detected via each
    // shooter's shotCount), then age and cull the live bolts. Spawns only while
    // the sim is stepping; keeps fading afterward so bolts finish cleanly.
    void updateLasers(float dtSeconds);
    // Laser-shooter items: each live (fading) laser bolt to the cell it just
    // captured, plus a faceted crystal token (dim grey while un-owned). Tokens are
    // driven by HexWorld::shooters(); bolts by lasers_; animTime_ bobs the gem.
    void drawShooters(Canvas& canvas) const;
    // Slowing-totem items: a cloud-on-a-cup token for every totem, plus a batched
    // mesh of falling snowflakes over the field of each captured (owned) one.
    void drawSlowTotems(Canvas& canvas);
    void drawCloudTotem(Canvas& canvas, hexanaut::Vec2 worldCenter, int phase) const;
    // Append one 6-pointed snowflake (3 crossed bars) into the shared mesh buffers.
    void appendSnowflake(float sx, float sy, float size, Color color);
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

    // A single fired shooter bolt: a world-space ray from the shooter cell to the
    // cell it captured, in its owner's color, that fades to nothing over `life`.
    // Decoupled from the shooter once spawned (it is a one-shot muzzle flash).
    struct Laser {
        hexanaut::Vec2 from;
        hexanaut::Vec2 to;
        hexanaut::PlayerId owner = 0;
        float age = 0.0F;
        float life = 0.4F;
    };

    SceneManager& manager_;
    Difficulty difficulty_;
    hexanaut::HexWorld world_;

    float accum_ = 0.0F;
    float camX_ = 0.0F;
    float camY_ = 0.0F;
    float zoom_ = 1.0F;
    float animTime_ = 0.0F; // free-running clock for cosmetic FX (laser pulse, gem bob)

    hexanaut::Vec2 aimScreen_; // last pointer position (logical px) used to steer
    bool hasAim_ = false;
    bool backPressed_ = false;

    Phase phase_ = Phase::Playing;
    bool recorded_ = false;
    float finalPercent_ = 0.0F;
    float bestPercent_ = 0.0F;
    // The player's territory % from the last tick it was alive. Dying frees its
    // territory to 0 inside the sim, so game-over reads this snapshot (taken before
    // the fatal step) instead of the now-zeroed live value.
    float lastLivePercent_ = 0.0F;

    // Reused geometry buffers so the whole field submits in one fillMesh call.
    std::vector<Canvas::Vertex> meshVerts_;
    std::vector<int> meshIdx_;
    std::vector<PowerupDraw> powerupDraws_;   // power-up cells found during drawField
    std::vector<TrailOutline> trailOutlines_; // active-trail cells found during drawField
    std::vector<ScreenPos> ropeScratch_;      // reused per-player rope path in drawTrails

    std::vector<Particle> particles_; // live cut-through sparks
    float fxSpawnAccum_ = 0.0F;       // dt accumulator pacing spark bursts
    std::mt19937 fxRng_;              // visual-only jitter (fixed seed; not gameplay)

    std::vector<Laser> lasers_;            // live (fading) shooter bolts
    std::vector<std::uint32_t> shotSeen_;  // last-seen shotCount per shooter (parallel to shooters())

    Button homeButton_;
    Button retryButton_;
};

} // namespace og
