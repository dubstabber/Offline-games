#include "games/hole/HoleScene.hpp"

#include "core/Canvas.hpp"
#include "core/Color.hpp"
#include "core/FixedTimestep.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "games/hole/HoleConfig.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace og {
namespace {

namespace cfg = hole::config;
using hole::CityObject;
using hole::ObjectKind;
using hole::Vec2;

constexpr float kHalfW = layout::kWidthF * 0.5F;
constexpr float kHalfH = layout::kHeightF * 0.5F;

constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;
constexpr float kButtonRowY = 820.0F;

constexpr float kBaseZoom = 1.14F;
constexpr float kMinZoom = 0.42F;
constexpr float kMaxZoom = 1.22F;
constexpr float kZoomExp = 0.45F;
constexpr float kZoomRate = 3.4F;
constexpr float kFollowRate = 10.0F;

constexpr float kRoadW = 260.0F;
constexpr float kRoadHalf = kRoadW * 0.5F;
constexpr std::array<float, 3> kRoads{900.0F, 1800.0F, 2700.0F};

constexpr Color kOutside = rgb(126, 136, 145);
constexpr Color kSidewalk = rgb(185, 193, 202);
constexpr Color kBlock = rgb(154, 184, 145);
constexpr Color kRoad = rgb(70, 77, 91);
constexpr Color kRoadLine = rgb(228, 218, 164, 120);
constexpr Color kCrosswalk = rgb(232, 236, 238, 180);
constexpr Color kShadow = rgb(0, 0, 0, 45);
constexpr Color kHoleBlack = rgb(6, 8, 13);
constexpr Color kHoleInner = rgb(0, 0, 3);
constexpr Color kHoleRing = rgb(31, 168, 226, 220);
constexpr Color kHudPanel = rgb(16, 18, 24, 210);
constexpr Color kHudText = rgb(244, 247, 252);
constexpr Color kHudMuted = rgb(174, 185, 196);

[[nodiscard]] int difficultyToIndex(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return 0;
    case Difficulty::Hard:
        return 2;
    case Difficulty::Medium:
    case Difficulty::VeryHard:
        break;
    }
    return 1;
}

[[nodiscard]] int& holeBestField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.holeBestEasy;
    case Difficulty::Hard:
        return s.holeBestHard;
    case Difficulty::Medium:
    case Difficulty::VeryHard:
        break;
    }
    return s.holeBestMedium;
}

void fillOrientedBox(Canvas& canvas, float cx, float cy, float ux, float uy, float halfLen,
                     float halfWid, Color fillColor) {
    const float sx = -uy;
    const float sy = ux;
    const std::array<Canvas::Vertex, 4> verts{{
        {.x = cx + (ux * halfLen) + (sx * halfWid),
         .y = cy + (uy * halfLen) + (sy * halfWid),
         .color = fillColor},
        {.x = cx - (ux * halfLen) + (sx * halfWid),
         .y = cy - (uy * halfLen) + (sy * halfWid),
         .color = fillColor},
        {.x = cx - (ux * halfLen) - (sx * halfWid),
         .y = cy - (uy * halfLen) - (sy * halfWid),
         .color = fillColor},
        {.x = cx + (ux * halfLen) - (sx * halfWid),
         .y = cy + (uy * halfLen) - (sy * halfWid),
         .color = fillColor},
    }};
    canvas.fillConvexPolygon(verts);
}

[[nodiscard]] Vec2 vehicleDir(const CityObject& object) {
    if (object.mobile) {
        const Vec2 dir = hole::normalize(object.pathB - object.pathA);
        if (hole::lengthSq(dir) > 0.01F) {
            return dir;
        }
    }
    return {1.0F, 0.0F};
}

} // namespace

HoleScene::HoleScene(SceneManager& manager, Difficulty difficulty)
    : manager_(manager), difficulty_(difficulty),
      world_(difficultyToIndex(difficulty), std::random_device{}()), camX_(world_.player().pos.x),
      camY_(world_.player().pos.y),
      backButton_(IconButton::Icon::Chevron, kBackCx, kBackCy, kBackRadius),
      bestScore_(holeBestField(settings(), difficulty)),
      overlay_(color(difficulty_), colors::white, kButtonRowY) {
    backButton_.setOnTap([this] { manager_.pop(); });
    overlay_.setOnHome([this] { manager_.popToRoot(); });
    overlay_.setActionLabel("REPLAY");
    overlay_.setOnAction(
        [this] { manager_.replace(std::make_unique<HoleScene>(manager_, difficulty_)); });
}

// ---- Input ------------------------------------------------------------------

void HoleScene::handleSteer(const PointerEvent& event) {
    switch (event.phase) {
    case PointerEvent::Phase::Down:
    case PointerEvent::Phase::Move:
        aimScreen_ = {event.x, event.y};
        pointerActive_ = true;
        break;
    case PointerEvent::Phase::Up:
        pointerActive_ = false;
        break;
    }
}

void HoleScene::handleInput(const PointerEvent& event) {
    if (backButton_.handleInput(event)) {
        return;
    }
    if (phase_ == Phase::Complete) {
        overlay_.handleInput(event);
        return;
    }
    handleSteer(event);
}

// ---- Update -----------------------------------------------------------------

void HoleScene::update(float dtSeconds) {
    if (phase_ == Phase::Playing) {
        const hole::PlayerInput input{
            .aimWorld =
                pointerActive_ ? screenToWorld(aimScreen_.x, aimScreen_.y) : world_.player().pos,
            .active = pointerActive_,
        };
        world_.setPlayerInput(input);
        advanceFixed(accum_, dtSeconds, cfg::kFixedDt, cfg::kMaxAccumDt, [this] { world_.step(); });
        if (world_.completed()) {
            enterComplete();
        }
    }
    updateCamera(dtSeconds);
}

void HoleScene::enterComplete() {
    if (recorded_) {
        return;
    }
    phase_ = Phase::Complete;
    finalScore_ = world_.playerScore();
    bestScore_ = std::max(bestScore_, finalScore_);
    Settings& s = settings();
    holeBestField(s, difficulty_) = bestScore_;
    saveSettings(s);
    recorded_ = true;
}

void HoleScene::updateCamera(float dtSeconds) {
    const hole::HolePlayer& p = world_.player();
    const float follow = 1.0F - std::exp(-kFollowRate * dtSeconds);
    camX_ += (p.pos.x - camX_) * follow;
    camY_ += (p.pos.y - camY_) * follow;
    const float targetZoom = std::clamp(
        kBaseZoom * std::pow(32.0F / std::max(p.radius, 1.0F), kZoomExp), kMinZoom, kMaxZoom);
    const float ease = 1.0F - std::exp(-kZoomRate * dtSeconds);
    zoom_ += (targetZoom - zoom_) * ease;
}

// ---- Camera transform -------------------------------------------------------

hole::Vec2 HoleScene::screenToWorld(float sx, float sy) const {
    return {((sx - kHalfW) / zoom_) + camX_, ((sy - kHalfH) / zoom_) + camY_};
}

HoleScene::ScreenPos HoleScene::toScreen(hole::Vec2 world) const {
    return {.x = ((world.x - camX_) * zoom_) + kHalfW, .y = ((world.y - camY_) * zoom_) + kHalfH};
}

bool HoleScene::onScreen(float sx, float sy, float radius) {
    return sx + radius >= 0.0F && sx - radius <= layout::kWidthF && sy + radius >= 0.0F &&
           sy - radius <= layout::kHeightF;
}

// ---- Rendering --------------------------------------------------------------

void HoleScene::drawWorldRect(Canvas& canvas, float x, float y, float w, float h,
                              Color fillColor) const {
    const ScreenPos p = toScreen({x, y});
    canvas.fillRect(p.x, p.y, w * zoom_, h * zoom_, fillColor);
}

void HoleScene::drawCityBase(Canvas& canvas) const {
    canvas.clear(kOutside);
    drawWorldRect(canvas, 0.0F, 0.0F, hole::HoleWorld::worldW(), hole::HoleWorld::worldH(),
                  kSidewalk);

    const std::array<float, 4> starts{35.0F, 1030.0F, 1930.0F, 2830.0F};
    const std::array<float, 4> sizes{735.0F, 640.0F, 640.0F, 735.0F};
    for (std::size_t ix = 0; ix < starts.size(); ++ix) {
        for (std::size_t iy = 0; iy < starts.size(); ++iy) {
            drawWorldRect(canvas, starts.at(ix), starts.at(iy), sizes.at(ix), sizes.at(iy), kBlock);
        }
    }

    for (const float road : kRoads) {
        drawWorldRect(canvas, road - kRoadHalf, 0.0F, kRoadW, hole::HoleWorld::worldH(), kRoad);
        drawWorldRect(canvas, 0.0F, road - kRoadHalf, hole::HoleWorld::worldW(), kRoadW, kRoad);
    }

    // Lane markers.
    for (const float road : kRoads) {
        const ScreenPos h0 = toScreen({0.0F, road});
        const ScreenPos h1 = toScreen({hole::HoleWorld::worldW(), road});
        canvas.line(h0.x, h0.y, h1.x, h1.y, std::max(2.0F, 4.0F * zoom_), kRoadLine);
        const ScreenPos v0 = toScreen({road, 0.0F});
        const ScreenPos v1 = toScreen({road, hole::HoleWorld::worldH()});
        canvas.line(v0.x, v0.y, v1.x, v1.y, std::max(2.0F, 4.0F * zoom_), kRoadLine);
    }

    // Crosswalk stripes at every intersection.
    for (const float rx : kRoads) {
        for (const float ry : kRoads) {
            for (int i = 0; i < 5; ++i) {
                const float off = -80.0F + (static_cast<float>(i) * 40.0F);
                drawWorldRect(canvas, rx - 112.0F, ry + off - 8.0F, 224.0F, 16.0F, kCrosswalk);
                drawWorldRect(canvas, rx + off - 8.0F, ry - 112.0F, 16.0F, 224.0F, kCrosswalk);
            }
        }
    }

    const ScreenPos a = toScreen({0.0F, 0.0F});
    const ScreenPos b = toScreen({hole::HoleWorld::worldW(), 0.0F});
    const ScreenPos c = toScreen({hole::HoleWorld::worldW(), hole::HoleWorld::worldH()});
    const ScreenPos d = toScreen({0.0F, hole::HoleWorld::worldH()});
    const float thick = std::max(3.0F, 8.0F * zoom_);
    canvas.line(a.x, a.y, b.x, b.y, thick, rgb(90, 98, 108));
    canvas.line(b.x, b.y, c.x, c.y, thick, rgb(90, 98, 108));
    canvas.line(c.x, c.y, d.x, d.y, thick, rgb(90, 98, 108));
    canvas.line(d.x, d.y, a.x, a.y, thick, rgb(90, 98, 108));
}

void HoleScene::drawObjects(Canvas& canvas) const {
    std::vector<std::size_t> order(world_.objects().size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::sort(order, [this](std::size_t a, std::size_t b) {
        return world_.objects().at(a).pos.y < world_.objects().at(b).pos.y;
    });
    for (const std::size_t index : order) {
        const CityObject& object = world_.objects().at(index);
        if (!object.consumed) {
            drawObject(canvas, object);
        }
    }
}

void HoleScene::drawObject(Canvas& canvas, const CityObject& object) const {
    const ScreenPos sp = toScreen(object.pos);
    const float r = std::max(3.0F, object.solidRadius * zoom_);
    if (!onScreen(sp.x, sp.y, r * 2.4F)) {
        return;
    }

    if (object.kind != ObjectKind::Cone) {
        canvas.fillCircle(sp.x + (4.0F * zoom_), sp.y + (6.0F * zoom_), r * 0.9F, kShadow);
    }

    switch (object.kind) {
    case ObjectKind::Cone: {
        const std::array<Canvas::Vertex, 3> tri{{
            {.x = sp.x, .y = sp.y - (r * 1.15F), .color = rgb(245, 132, 39)},
            {.x = sp.x - (r * 0.95F), .y = sp.y + (r * 0.78F), .color = rgb(225, 92, 31)},
            {.x = sp.x + (r * 0.95F), .y = sp.y + (r * 0.78F), .color = rgb(255, 174, 62)},
        }};
        canvas.fillConvexPolygon(tri);
        canvas.line(sp.x - (r * 0.45F), sp.y + (r * 0.12F), sp.x + (r * 0.45F), sp.y + (r * 0.12F),
                    std::max(2.0F, r * 0.18F), colors::white);
        break;
    }
    case ObjectKind::Pedestrian:
        canvas.fillCircle(sp.x, sp.y + (r * 0.22F), r * 0.68F, rgb(70, 115, 210));
        canvas.fillCircle(sp.x, sp.y - (r * 0.52F), r * 0.42F, rgb(242, 190, 135));
        break;
    case ObjectKind::TrashCan:
        canvas.fillRoundedRect(sp.x - (r * 0.68F), sp.y - r, r * 1.36F, r * 2.0F, r * 0.22F,
                               rgb(45, 126, 92));
        canvas.fillRoundedRect(sp.x - (r * 0.78F), sp.y - (r * 1.08F), r * 1.56F, r * 0.28F,
                               r * 0.12F, rgb(32, 90, 68));
        canvas.line(sp.x - (r * 0.36F), sp.y - (r * 0.52F), sp.x - (r * 0.36F), sp.y + (r * 0.62F),
                    std::max(2.0F, r * 0.08F), rgb(35, 96, 72));
        canvas.line(sp.x + (r * 0.36F), sp.y - (r * 0.52F), sp.x + (r * 0.36F), sp.y + (r * 0.62F),
                    std::max(2.0F, r * 0.08F), rgb(35, 96, 72));
        break;
    case ObjectKind::Bench:
        canvas.fillRoundedRect(sp.x - (r * 1.05F), sp.y - (r * 0.38F), r * 2.1F, r * 0.76F,
                               r * 0.18F, rgb(142, 83, 43));
        canvas.line(sp.x - (r * 0.86F), sp.y - (r * 0.12F), sp.x + (r * 0.86F), sp.y - (r * 0.12F),
                    std::max(2.0F, r * 0.10F), rgb(91, 54, 34));
        canvas.line(sp.x - (r * 0.86F), sp.y + (r * 0.16F), sp.x + (r * 0.86F), sp.y + (r * 0.16F),
                    std::max(2.0F, r * 0.10F), rgb(91, 54, 34));
        break;
    case ObjectKind::Tree:
        canvas.fillCircle(sp.x, sp.y + (r * 0.52F), r * 0.38F, rgb(110, 74, 44));
        canvas.fillCircle(sp.x - (r * 0.34F), sp.y - (r * 0.10F), r * 0.72F, rgb(67, 169, 72));
        canvas.fillCircle(sp.x + (r * 0.34F), sp.y - (r * 0.10F), r * 0.72F, rgb(95, 202, 74));
        canvas.fillCircle(sp.x, sp.y - (r * 0.44F), r * 0.78F, rgb(119, 221, 81));
        break;
    case ObjectKind::Stand:
        canvas.fillRoundedRect(sp.x - r, sp.y - (r * 0.65F), r * 2.0F, r * 1.3F, r * 0.18F,
                               rgb(232, 196, 116));
        canvas.fillRoundedRect(sp.x - (r * 1.08F), sp.y - (r * 0.95F), r * 2.16F, r * 0.55F,
                               r * 0.14F, rgb(228, 70, 74));
        canvas.fillRect(sp.x - (r * 0.36F), sp.y - (r * 0.95F), r * 0.36F, r * 0.55F,
                        colors::white);
        canvas.fillRect(sp.x + (r * 0.36F), sp.y - (r * 0.95F), r * 0.36F, r * 0.55F,
                        colors::white);
        break;
    case ObjectKind::Car:
    case ObjectKind::Van:
        drawVehicle(canvas, object, sp, r);
        break;
    case ObjectKind::Kiosk:
    case ObjectKind::SmallBuilding:
    case ObjectKind::Apartment:
    case ObjectKind::Tower:
        drawBuilding(canvas, object, sp, r);
        break;
    }
}

void HoleScene::drawBuilding(Canvas& canvas, const CityObject& object, ScreenPos sp,
                             float r) const {
    float w = r * 2.0F;
    float h = r * 1.8F;
    Color roof = rgb(178, 92, 82);
    Color trim = rgb(128, 62, 64);
    int cols = 2;
    int rows = 2;
    if (object.kind == ObjectKind::SmallBuilding) {
        w = r * 2.25F;
        h = r * 1.95F;
        roof = rgb(190, 105, 86);
        trim = rgb(138, 72, 62);
        cols = 3;
        rows = 3;
    } else if (object.kind == ObjectKind::Apartment) {
        w = r * 2.25F;
        h = r * 2.2F;
        roof = rgb(194, 164, 104);
        trim = rgb(128, 104, 82);
        cols = 4;
        rows = 4;
    } else if (object.kind == ObjectKind::Tower) {
        w = r * 2.45F;
        h = r * 2.45F;
        roof = rgb(80, 106, 132);
        trim = rgb(50, 70, 95);
        cols = 5;
        rows = 5;
    }

    canvas.fillRoundedRect(sp.x - (w * 0.5F) + (8.0F * zoom_), sp.y - (h * 0.5F) + (10.0F * zoom_),
                           w, h, r * 0.14F, kShadow);
    canvas.fillRoundedRect(sp.x - (w * 0.5F), sp.y - (h * 0.5F), w, h, r * 0.14F, trim);
    canvas.fillRoundedRect(sp.x - (w * 0.44F), sp.y - (h * 0.44F), w * 0.88F, h * 0.88F, r * 0.10F,
                           roof);

    const float winW = w * 0.10F;
    const float winH = h * 0.08F;
    for (int ix = 0; ix < cols; ++ix) {
        for (int iy = 0; iy < rows; ++iy) {
            const float fx = (static_cast<float>(ix) + 0.5F) / static_cast<float>(cols);
            const float fy = (static_cast<float>(iy) + 0.5F) / static_cast<float>(rows);
            canvas.fillRoundedRect(sp.x - (w * 0.35F) + (fx * w * 0.70F) - (winW * 0.5F),
                                   sp.y - (h * 0.34F) + (fy * h * 0.68F) - (winH * 0.5F), winW,
                                   winH, 2.0F * zoom_, rgb(174, 219, 229, 170));
        }
    }
}

void HoleScene::drawVehicle(Canvas& canvas, const CityObject& object, ScreenPos sp, float r) const {
    const Vec2 dir = vehicleDir(object);
    const float ux = dir.x;
    const float uy = dir.y;
    const bool van = object.kind == ObjectKind::Van;
    const float halfLen = r * (van ? 1.34F : 1.10F);
    const float halfWid = r * (van ? 0.58F : 0.48F);
    const Color body = van ? rgb(230, 230, 218) : rgb(226, 82, 76);
    const Color nose = van ? rgb(94, 150, 190) : rgb(245, 178, 64);
    fillOrientedBox(canvas, sp.x + (5.0F * zoom_), sp.y + (6.0F * zoom_), ux, uy, halfLen, halfWid,
                    kShadow);
    fillOrientedBox(canvas, sp.x, sp.y, ux, uy, halfLen, halfWid, body);
    fillOrientedBox(canvas, sp.x + (ux * halfLen * 0.35F), sp.y + (uy * halfLen * 0.35F), ux, uy,
                    halfLen * 0.28F, halfWid * 0.72F, rgb(112, 183, 215));
    fillOrientedBox(canvas, sp.x + (ux * halfLen * 0.84F), sp.y + (uy * halfLen * 0.84F), ux, uy,
                    halfLen * 0.12F, halfWid * 0.92F, nose);
}

void HoleScene::drawHole(Canvas& canvas) const {
    const hole::HolePlayer& p = world_.player();
    const ScreenPos sp = toScreen(p.pos);
    const float r = p.radius * zoom_;
    canvas.fillCircle(sp.x + (6.0F * zoom_), sp.y + (8.0F * zoom_), r * 1.04F, kShadow);
    canvas.fillCircle(sp.x, sp.y, r + std::max(4.0F, 8.0F * zoom_), kHoleRing);
    canvas.fillCircle(sp.x, sp.y, r, kHoleBlack);
    canvas.fillCircle(sp.x, sp.y, r * 0.62F, kHoleInner);
}

void HoleScene::drawHud(Canvas& canvas) const {
    backButton_.render(canvas);

    constexpr float kPanelW = 330.0F;
    constexpr float kPanelH = 124.0F;
    constexpr float kPanelX = (layout::kWidthF - kPanelW) * 0.5F;
    constexpr float kPanelY = 36.0F;
    canvas.fillRoundedRect(kPanelX, kPanelY, kPanelW, kPanelH, 18.0F, kHudPanel);
    canvas.textCentered(label(difficulty_), layout::kWidthF * 0.5F, kPanelY + 26.0F, 22.0F,
                        color(difficulty_));
    canvas.textCentered(std::to_string(world_.playerScore()), layout::kWidthF * 0.5F,
                        kPanelY + 68.0F, 50.0F, kHudText);
    const int pct = static_cast<int>(std::lround(world_.completionPercent()));
    canvas.textCentered("CLEAR " + std::to_string(pct) + "%", layout::kWidthF * 0.5F,
                        kPanelY + 108.0F, 24.0F, kHudMuted);

    canvas.text("BEST", layout::kWidthF - 28.0F, 54.0F, 22.0F, kHudMuted, Canvas::Align::Right);
    canvas.text(std::to_string(bestScore_), layout::kWidthF - 28.0F, 84.0F, 30.0F, kHudText,
                Canvas::Align::Right);
}

void HoleScene::drawOverlay(Canvas& canvas) const {
    overlay_.render(canvas, "CITY CLEAR", 520.0F, 78.0F);
    canvas.textCentered("SCORE  " + std::to_string(finalScore_), layout::kWidthF / 2.0F, 632.0F,
                        44.0F, colors::white);
    canvas.textCentered("BEST  " + std::to_string(bestScore_), layout::kWidthF / 2.0F, 692.0F,
                        34.0F, colors::botCyan);
}

void HoleScene::render(Canvas& canvas) {
    drawCityBase(canvas);
    drawObjects(canvas);
    drawHole(canvas);
    drawHud(canvas);
    if (phase_ == Phase::Complete) {
        drawOverlay(canvas);
    }
}

} // namespace og
