#include "games/hexanaut/HexanautScene.hpp"

#include "core/Canvas.hpp"
#include "core/Color.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"
#include "games/hexanaut/HexConfig.hpp"
#include "games/hexanaut/HexGrid.hpp"
#include "games/hexanaut/HexPalette.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <numbers>
#include <random>
#include <string>

namespace og {
namespace {

namespace cfg = hexanaut::config;
namespace pal = hexanaut::palette;
using hexanaut::Cell;
using hexanaut::HexCoord;
using hexanaut::HexDir;
using hexanaut::Player;
using hexanaut::Vec2;

constexpr float kHalfW = layout::kWidthF * 0.5F;
constexpr float kHalfH = layout::kHeightF * 0.5F;

// Back button chrome (matches the other games).
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;

// Game-over overlay buttons — same row layout as the other games.
constexpr float kButtonRowY = 820.0F;
constexpr float kHomeSize = 140.0F;
constexpr float kRetryW = 360.0F;
constexpr float kButtonGap = 24.0F;
constexpr float kRowWidth = kHomeSize + kButtonGap + kRetryW;
constexpr float kRowX = (layout::kWidthF - kRowWidth) / 2.0F;
constexpr const char* kHome = "\xF0\x9F\x8F\xA0"; // 🏠

constexpr float kFollowRate = cfg::kFollowRate;

// Edge k of a hex (corner k -> corner k+1) faces this neighbor direction.
constexpr std::array<HexDir, 6> kEdgeToDir{HexDir::SE, HexDir::S, HexDir::SW,
                                           HexDir::NW, HexDir::N, HexDir::NE};

const std::array<Vec2, 6>& cornerUnits() {
    static const std::array<Vec2, 6> units = [] {
        std::array<Vec2, 6> a{};
        for (int k = 0; k < 6; ++k) {
            const float ang = static_cast<float>(k) * 60.0F * std::numbers::pi_v<float> / 180.0F;
            a.at(static_cast<std::size_t>(k)) = Vec2{std::cos(ang), std::sin(ang)};
        }
        return a;
    }();
    return units;
}

[[nodiscard]] Vec2 cornerOffset(int k, float inset) {
    return cornerUnits().at(static_cast<std::size_t>(k)) * (cfg::kHexSize * inset);
}

[[nodiscard]] int difficultyToIndex(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return 0;
    case Difficulty::Hard:
        return 2;
    case Difficulty::Medium:
    case Difficulty::VeryHard:
        break; // three difficulties; VeryHard folds to Medium
    }
    return 1;
}

[[nodiscard]] std::string formatPercent(float pct) {
    std::array<char, 16> buf{};
    std::snprintf(buf.data(), buf.size(), "%.1f%%", static_cast<double>(pct));
    return std::string{buf.data()};
}

// Best territory is persisted per difficulty in tenths of a percent.
[[nodiscard]] int& hexanautBestField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.hexanautBestEasy;
    case Difficulty::Hard:
        return s.hexanautBestHard;
    case Difficulty::Medium:
    case Difficulty::VeryHard:
        break;
    }
    return s.hexanautBestMedium;
}

} // namespace

HexanautScene::HexanautScene(SceneManager& manager, Difficulty difficulty)
    : manager_(manager), difficulty_(difficulty),
      world_(difficultyToIndex(difficulty), std::random_device{}()),
      bestPercent_(static_cast<float>(hexanautBestField(settings(), difficulty)) / 10.0F),
      homeButton_(kHome, kRowX, kButtonRowY, kHomeSize, kHomeSize),
      retryButton_("RETRY", kRowX + kHomeSize + kButtonGap, kButtonRowY, kRetryW, kHomeSize) {
    const Vec2 c = avatarWorld(world_.player());
    camX_ = c.x;
    camY_ = c.y;

    homeButton_.setColors(colors::white, colors::panelBrown);
    homeButton_.setOnTap([this] { manager_.popToRoot(); });
    retryButton_.setColors(color(difficulty_), colors::white);
    retryButton_.setOnTap(
        [this] { manager_.replace(std::make_unique<HexanautScene>(manager_, difficulty_)); });
}

// ---- Camera transform -------------------------------------------------------

HexanautScene::ScreenPos HexanautScene::toScreen(Vec2 world, float lift) const {
    return {.x = ((world.x - camX_) * zoom_) + kHalfW,
            .y = ((world.y - camY_) * cfg::kSquash * zoom_) + kHalfH - (lift * zoom_)};
}

Vec2 HexanautScene::screenToWorld(float sx, float sy) const {
    return {((sx - kHalfW) / zoom_) + camX_, ((sy - kHalfH) / (cfg::kSquash * zoom_)) + camY_};
}

Vec2 HexanautScene::avatarWorld(const Player& p) {
    return hexanaut::lerp(hexanaut::axialToWorld(p.fromCell, cfg::kHexSize),
                          hexanaut::axialToWorld(p.cell, cfg::kHexSize), p.stepProgress);
}

// ---- Input ------------------------------------------------------------------

bool HexanautScene::handleBackButton(const PointerEvent& event) {
    if (event.phase == PointerEvent::Phase::Move) {
        return false;
    }
    const bool inside = hitTest(event, kBackCx - kBackRadius, kBackCy - kBackRadius,
                                kBackRadius * 2.0F, kBackRadius * 2.0F);
    if (event.phase == PointerEvent::Phase::Down) {
        backPressed_ = inside;
        return inside;
    }
    const bool wasPressed = backPressed_;
    backPressed_ = false;
    if (wasPressed && inside) {
        manager_.pop();
        return true;
    }
    return false;
}

void HexanautScene::handleSteer(const PointerEvent& event) {
    if (event.phase == PointerEvent::Phase::Up) {
        return; // keep the last heading after the finger lifts
    }
    aimScreen_ = {event.x, event.y};
    hasAim_ = true;
}

void HexanautScene::handleInput(const PointerEvent& event) {
    if (handleBackButton(event)) {
        return;
    }
    if (phase_ == Phase::GameOver) {
        if (homeButton_.handleInput(event)) {
            return;
        }
        retryButton_.handleInput(event);
        return;
    }
    handleSteer(event);
}

// ---- Update -----------------------------------------------------------------

void HexanautScene::update(float dtSeconds) {
    if (phase_ == Phase::Playing) {
        if (hasAim_) {
            const Vec2 aim = screenToWorld(aimScreen_.x, aimScreen_.y);
            const Vec2 dir = aim - avatarWorld(world_.player());
            if (hexanaut::length(dir) > 4.0F) {
                world_.setPlayerDesiredDir(hexanaut::quantizeToHexDir(hexanaut::angleOf(dir)));
            }
        }
        accum_ += std::min(dtSeconds, cfg::kMaxAccumDt);
        while (accum_ >= cfg::kFixedDt) {
            world_.step();
            accum_ -= cfg::kFixedDt;
        }
        if (!world_.playerAlive()) {
            enterGameOver();
        }
    }
    updateCamera(dtSeconds);
}

void HexanautScene::enterGameOver() {
    if (recorded_) {
        return;
    }
    phase_ = Phase::GameOver;
    finalPercent_ = world_.playerPercent();
    bestPercent_ = std::max(bestPercent_, finalPercent_);
    Settings& s = settings();
    hexanautBestField(s, difficulty_) = static_cast<int>(std::lround(bestPercent_ * 10.0F));
    saveSettings(s);
    recorded_ = true;
}

void HexanautScene::updateCamera(float dtSeconds) {
    const Vec2 a = avatarWorld(world_.player());
    const float follow = 1.0F - std::exp(-kFollowRate * dtSeconds);
    camX_ += (a.x - camX_) * follow;
    camY_ += (a.y - camY_) * follow;
    // The Vision power-up zooms the camera out for a wider view.
    const float targetZoom = world_.player().visionTimer > 0.0F ? cfg::kVisionZoom : cfg::kBaseZoom;
    const float zoomEase = 1.0F - std::exp(-4.0F * dtSeconds);
    zoom_ += (targetZoom - zoom_) * zoomEase;
}

// ---- Rendering --------------------------------------------------------------

void HexanautScene::appendHexTop(Vec2 centerWorld, float inset, float lift, Color color) {
    const auto base = static_cast<int>(meshVerts_.size());
    const ScreenPos cs = toScreen(centerWorld, lift);
    meshVerts_.push_back({.x = cs.x, .y = cs.y, .color = color});
    for (int k = 0; k < 6; ++k) {
        const ScreenPos sp = toScreen(centerWorld + cornerOffset(k, inset), lift);
        meshVerts_.push_back({.x = sp.x, .y = sp.y, .color = color});
    }
    for (int k = 0; k < 6; ++k) {
        meshIdx_.push_back(base);
        meshIdx_.push_back(base + 1 + k);
        meshIdx_.push_back(base + 1 + ((k + 1) % 6));
    }
}

void HexanautScene::appendWall(Vec2 centerWorld, int edge, float liftTop, Color top, Color bottom) {
    const int a = edge;
    const int b = (edge + 1) % 6;
    const ScreenPos topA = toScreen(centerWorld + cornerOffset(a, 1.0F), liftTop);
    const ScreenPos topB = toScreen(centerWorld + cornerOffset(b, 1.0F), liftTop);
    const ScreenPos botB = toScreen(centerWorld + cornerOffset(b, 1.0F), 0.0F);
    const ScreenPos botA = toScreen(centerWorld + cornerOffset(a, 1.0F), 0.0F);
    const auto base = static_cast<int>(meshVerts_.size());
    meshVerts_.push_back({.x = topA.x, .y = topA.y, .color = top});
    meshVerts_.push_back({.x = topB.x, .y = topB.y, .color = top});
    meshVerts_.push_back({.x = botB.x, .y = botB.y, .color = bottom});
    meshVerts_.push_back({.x = botA.x, .y = botA.y, .color = bottom});
    for (const int i : {0, 1, 2, 0, 2, 3}) {
        meshIdx_.push_back(base + i);
    }
}

void HexanautScene::drawField(Canvas& canvas) {
    meshVerts_.clear();
    meshIdx_.clear();
    powerupDraws_.clear();

    const hexanaut::HexGrid& grid = world_.grid();
    constexpr float kS = cfg::kHexSize;
    const float worldMargin = (2.0F * kS) + cfg::kTrailLift;
    const float wxMin = camX_ - (kHalfW / zoom_) - worldMargin;
    const float wxMax = camX_ + (kHalfW / zoom_) + worldMargin;
    const float wyMin = camY_ - (kHalfH / (cfg::kSquash * zoom_)) - worldMargin;
    const float wyMax = camY_ + (kHalfH / (cfg::kSquash * zoom_)) + worldMargin;

    const float invQ = 1.0F / (1.5F * kS);
    const float invR = 1.0F / (std::numbers::sqrt3_v<float> * kS);
    const int qMin = std::max(0, static_cast<int>(std::floor(wxMin * invQ)) - 1);
    const int qMax = std::min(grid.width() - 1, static_cast<int>(std::ceil(wxMax * invQ)) + 1);
    const int rMin = std::max(
        0, static_cast<int>(std::floor((wyMin * invR) - (static_cast<float>(qMax) * 0.5F))) - 1);
    const int rMax = std::min(
        grid.height() - 1,
        static_cast<int>(std::ceil((wyMax * invR) - (static_cast<float>(qMin) * 0.5F))) + 1);

    const float cullMargin = (2.0F * kS * zoom_) + (cfg::kTrailLift * zoom_);

    // Back-to-front: ascending r (then q) so nearer rows overlap farther ones.
    for (int r = rMin; r <= rMax; ++r) {
        for (int q = qMin; q <= qMax; ++q) {
            const HexCoord coord{q, r};
            if (!grid.contains(coord)) {
                continue;
            }
            const Vec2 center = hexanaut::axialToWorld(coord, kS);
            const ScreenPos cs = toScreen(center, 0.0F);
            if (cs.x < -cullMargin || cs.x > layout::kWidthF + cullMargin || cs.y < -cullMargin ||
                cs.y > layout::kHeightF + cullMargin) {
                continue;
            }
            appendCellPrism(grid, coord, center);
        }
    }

    canvas.fillMesh(meshVerts_, meshIdx_);
}

void HexanautScene::appendCellPrism(const hexanaut::HexGrid& grid, HexCoord coord, Vec2 center) {
    const Cell& cell = grid.at(coord);
    if (cell.trailOwner != hexanaut::kNoTrail) {
        // Active trail: a brighter prism riding a little higher than territory.
        const Color base = pal::lighten(pal::topColor(cell.trailOwner), 0.4F);
        const Color wallTop = pal::darken(base, pal::kWallTop);
        const Color wallBottom = pal::darken(base, pal::kWallBottom);
        for (int e = 0; e <= 2; ++e) {
            appendWall(center, e, cfg::kTrailLift, wallTop, wallBottom);
        }
        appendHexTop(center, 1.04F, cfg::kTrailLift, base);
    } else if (cell.owner != hexanaut::kNeutral) {
        // Owned territory: front-facing walls only where it borders a different cell.
        const Color base = pal::topColor(cell.owner);
        const Color wallTop = pal::darken(base, pal::kWallTop);
        const Color wallBottom = pal::darken(base, pal::kWallBottom);
        for (int e = 0; e <= 2; ++e) {
            const HexCoord nb =
                hexanaut::neighbor(coord, kEdgeToDir.at(static_cast<std::size_t>(e)));
            const bool boundary = !grid.contains(nb) || grid.at(nb).owner != cell.owner ||
                                  grid.at(nb).trailOwner != hexanaut::kNoTrail;
            if (boundary) {
                appendWall(center, e, cfg::kPrismLift, wallTop, wallBottom);
            }
        }
        appendHexTop(center, 1.04F, cfg::kPrismLift, base);
    } else {
        appendHexTop(center, cfg::kGroundInset, 0.0F, pal::kGround);
    }
    if (cell.powerup != 0) {
        powerupDraws_.push_back({.center = center, .type = cell.powerup});
    }
}

void HexanautScene::drawPowerups(Canvas& canvas) const {
    constexpr const char* kBolt = "\xE2\x9A\xA1";    // ⚡ Speed
    constexpr const char* kEye = "\xF0\x9F\x91\x81"; // 👁 Vision
    for (const PowerupDraw& pd : powerupDraws_) {
        const ScreenPos sp = toScreen(pd.center, cfg::kPrismLift + 22.0F);
        const float rad = std::max(12.0F, cfg::kHexSize * zoom_ * 0.6F);
        canvas.fillCircle(sp.x, sp.y, rad, rgb(20, 22, 30, 210));
        canvas.fillCircle(sp.x, sp.y, rad - 3.0F, rgb(248, 248, 252, 235));
        const bool speed = pd.type == static_cast<std::uint8_t>(hexanaut::PowerUp::Speed);
        canvas.emojiCentered(speed ? kBolt : kEye, sp.x, sp.y, rad * 1.4F);
    }
}

void HexanautScene::drawAvatars(Canvas& canvas) const {
    for (const Player& p : world_.players()) {
        if (!p.alive) {
            continue;
        }
        const ScreenPos hp = toScreen(avatarWorld(p), cfg::kTrailLift + 6.0F);
        const float r = std::max(6.0F, cfg::kHexSize * zoom_ * 0.5F);
        const Color body = pal::topColor(p.id);
        canvas.fillCircle(hp.x, hp.y, r + 3.0F, rgb(20, 22, 28));
        canvas.fillCircle(hp.x, hp.y, r, pal::lighten(body, 0.15F));
        canvas.textCentered(p.name, hp.x, hp.y - r - 16.0F, 22.0F,
                            p.isBot ? rgb(214, 218, 228) : colors::white);
    }
}

void HexanautScene::drawHud(Canvas& canvas) const {
    drawBackButton(canvas);
    const float cx = layout::kWidthF * 0.5F;
    canvas.textCentered(label(difficulty_), cx, 42.0F, 26.0F, color(difficulty_));
    canvas.textCentered(formatPercent(world_.playerPercent()), cx, 100.0F, 60.0F, colors::white);
    canvas.textCentered("KILLS " + std::to_string(world_.player().kills), cx, 150.0F, 22.0F,
                        colors::textMuted);
}

void HexanautScene::drawLeaderboard(Canvas& canvas) const {
    struct Row {
        const Player* p;
        float pct;
    };
    std::vector<Row> rows;
    rows.reserve(world_.players().size());
    int aliveCount = 0;
    for (const Player& p : world_.players()) {
        if (p.alive) {
            ++aliveCount;
        }
        rows.push_back({&p, world_.percent(p.id)});
    }
    std::ranges::sort(rows, [](const Row& a, const Row& b) { return a.pct > b.pct; });

    constexpr float kFs = 22.0F;
    constexpr float kRowH = 30.0F;
    constexpr int kMaxRows = 6;
    const float leftX = layout::kWidthF - 270.0F;
    const float rightX = layout::kWidthF - 18.0F;

    canvas.text(std::to_string(aliveCount) + " players", rightX, 116.0F, 20.0F, colors::textMuted,
                Canvas::Align::Right);

    const int shown = std::min<int>(kMaxRows, static_cast<int>(rows.size()));
    int youRank = 0;
    float y = 152.0F;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!rows.at(i).p->isBot) {
            youRank = static_cast<int>(i) + 1;
        }
    }
    for (int i = 0; i < shown; ++i) {
        const Row& row = rows.at(static_cast<std::size_t>(i));
        const bool you = !row.p->isBot;
        const Color c = you ? colors::white : pal::topColor(row.p->id);
        canvas.text(std::to_string(i + 1) + " " + row.p->name, leftX, y, kFs, c,
                    Canvas::Align::Left);
        canvas.text(formatPercent(row.pct), rightX, y, kFs, c, Canvas::Align::Right);
        y += kRowH;
    }
    if (youRank > shown) {
        y += 8.0F;
        canvas.text(std::to_string(youRank) + " YOU", leftX, y, kFs, colors::white,
                    Canvas::Align::Left);
        canvas.text(formatPercent(world_.playerPercent()), rightX, y, kFs, colors::white,
                    Canvas::Align::Right);
    }
}

void HexanautScene::drawMinimap(Canvas& canvas) {
    const hexanaut::HexGrid& grid = world_.grid();
    constexpr float kMap = 200.0F;
    const float boxX = layout::kWidthF - kMap - 20.0F;
    const float boxY = layout::kHeightF - kMap - 20.0F;
    canvas.fillRoundedRect(boxX - 6.0F, boxY - 6.0F, kMap + 12.0F, kMap + 12.0F, 12.0F,
                           rgb(16, 18, 24, 225));
    canvas.fillRoundedRect(boxX, boxY, kMap, kMap, 8.0F, rgb(34, 37, 45));

    const float cw = kMap / static_cast<float>(grid.width());
    const float ch = kMap / static_cast<float>(grid.height());
    const float half = std::max(1.4F, std::max(cw, ch) * 0.7F);

    // Owned cells batched into one mesh (the field buffer is free to reuse here).
    meshVerts_.clear();
    meshIdx_.clear();
    for (int r = 0; r < grid.height(); ++r) {
        for (int q = 0; q < grid.width(); ++q) {
            const hexanaut::Cell& cell = grid.at({q, r});
            if (cell.owner == hexanaut::kNeutral) {
                continue;
            }
            const float px = boxX + ((static_cast<float>(q) + 0.5F) * cw);
            const float py = boxY + ((static_cast<float>(r) + 0.5F) * ch);
            const Color col = pal::topColor(cell.owner);
            const auto base = static_cast<int>(meshVerts_.size());
            meshVerts_.push_back({.x = px - half, .y = py - half, .color = col});
            meshVerts_.push_back({.x = px + half, .y = py - half, .color = col});
            meshVerts_.push_back({.x = px + half, .y = py + half, .color = col});
            meshVerts_.push_back({.x = px - half, .y = py + half, .color = col});
            for (const int i : {0, 1, 2, 0, 2, 3}) {
                meshIdx_.push_back(base + i);
            }
        }
    }
    canvas.fillMesh(meshVerts_, meshIdx_);

    for (const Player& p : world_.players()) {
        if (!p.alive) {
            continue;
        }
        const float px = boxX + ((static_cast<float>(p.cell.q) + 0.5F) * cw);
        const float py = boxY + ((static_cast<float>(p.cell.r) + 0.5F) * ch);
        if (p.isBot) {
            canvas.fillCircle(px, py, 2.5F, pal::lighten(pal::topColor(p.id), 0.25F));
        } else {
            canvas.fillCircle(px, py, 5.0F, rgb(20, 22, 28));
            canvas.fillCircle(px, py, 3.5F, colors::white);
        }
    }
}

void HexanautScene::drawBackButton(Canvas& canvas) {
    canvas.fillCircle(kBackCx, kBackCy, kBackRadius, theme().backCircle);
    canvas.line(kBackCx + 12.0F, kBackCy - 24.0F, kBackCx - 14.0F, kBackCy, 14.0F, theme().chevron);
    canvas.line(kBackCx - 14.0F, kBackCy, kBackCx + 12.0F, kBackCy + 24.0F, 14.0F, theme().chevron);
}

void HexanautScene::drawOverlay(Canvas& canvas) const {
    canvas.fillRect(0.0F, 0.0F, layout::kWidthF, layout::kHeightF, colors::overlay);
    canvas.textCentered("GAME OVER", layout::kWidthF / 2.0F, 520.0F, 88.0F, colors::white);
    canvas.textCentered("TERRITORY  " + formatPercent(finalPercent_), layout::kWidthF / 2.0F,
                        632.0F, 40.0F, colors::white);
    canvas.textCentered("BEST  " + formatPercent(bestPercent_), layout::kWidthF / 2.0F, 692.0F,
                        32.0F, colors::botCyan);
    homeButton_.render(canvas);
    retryButton_.render(canvas);
}

void HexanautScene::render(Canvas& canvas) {
    canvas.clear(pal::kBackdrop);
    drawField(canvas);
    drawPowerups(canvas);
    drawAvatars(canvas);
    drawHud(canvas);
    drawLeaderboard(canvas);
    drawMinimap(canvas);
    if (phase_ == Phase::GameOver) {
        drawOverlay(canvas);
    }
}

} // namespace og
