#pragma once

#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/hole/HoleTypes.hpp"
#include "games/hole/HoleWorld.hpp"
#include "ui/IconButton.hpp"
#include "ui/ResultOverlay.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace og {

class SceneManager;

// A competitive Hole.io-style city. The SDL-free HoleWorld owns movement,
// growth, bot holes, object blocking, rival eating, and timed completion; this
// scene follows the player with a zooming camera, turns drag input into world
// aims, and renders the city from code-drawn roads, props, vehicles, buildings,
// and layered holes.
class HoleScene : public Scene {
public:
    HoleScene(SceneManager& manager, Difficulty difficulty);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    enum class Phase : std::uint8_t { Playing, Finished };

    struct ScreenPos {
        float x = 0.0F;
        float y = 0.0F;
    };

    void handleSteer(const PointerEvent& event);
    void enterFinished();
    void updateCamera(float dtSeconds);

    [[nodiscard]] hole::Vec2 screenToWorld(float sx, float sy) const;
    [[nodiscard]] ScreenPos toScreen(hole::Vec2 world) const;
    [[nodiscard]] static bool onScreen(float sx, float sy, float radius);

    void drawWorldRect(Canvas& canvas, float x, float y, float w, float h, Color fillColor) const;
    void drawCityBase(Canvas& canvas) const;
    void drawObjects(Canvas& canvas) const;
    void drawObject(Canvas& canvas, const hole::CityObject& object) const;
    void drawBuilding(Canvas& canvas, const hole::CityObject& object, ScreenPos sp, float r) const;
    void drawVehicle(Canvas& canvas, const hole::CityObject& object, ScreenPos sp, float r) const;
    void drawHoles(Canvas& canvas) const;
    void drawHole(Canvas& canvas, const hole::HolePlayer& hole, Color ring, bool isPlayer,
                  std::size_t index) const;
    void drawHud(Canvas& canvas) const;
    void drawOverlay(Canvas& canvas) const;

    SceneManager& manager_;
    Difficulty difficulty_;
    hole::HoleWorld world_;

    float accum_ = 0.0F;
    float camX_ = 0.0F;
    float camY_ = 0.0F;
    float zoom_ = 1.0F;

    hole::Vec2 aimScreen_;
    bool pointerActive_ = false;

    IconButton backButton_;
    Phase phase_ = Phase::Playing;
    bool recorded_ = false;
    int finalScore_ = 0;
    int bestScore_ = 0;
    int finalRank_ = 1;
    std::string resultTitle_;

    ResultOverlay overlay_;
};

} // namespace og
