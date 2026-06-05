#include "games/snake/SnakeScene.hpp"

#include "core/Canvas.hpp"
#include "core/Color.hpp"
#include "core/FixedTimestep.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"
#include "games/snake/SnakeConfig.hpp"
#include "games/snake/SnakePalette.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace og {
namespace {

using snake::Vec2;
namespace cfg = snake::config;

// ---- Chrome / HUD layout ----------------------------------------------------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;

constexpr float kBoostCx = layout::kWidthF - 120.0F;
constexpr float kBoostCy = layout::kHeightF - 170.0F;
constexpr float kBoostR = 92.0F;

// Game-over overlay row position.
constexpr float kButtonRowY = 820.0F;

constexpr const char* kBolt = "\xE2\x9A\xA1"; // ⚡

// ---- Camera tuning ----------------------------------------------------------
constexpr float kBaseZoom = 1.0F;
constexpr float kMinZoom = 0.52F;
constexpr float kMaxZoom = 1.18F;
constexpr float kZoomExp = 0.45F;
constexpr float kZoomRate = 3.0F;
constexpr float kFollowRate = 12.0F;

constexpr int kGhostCount = 220;
constexpr int kDrawStride = 2; // draw every Nth body sample (they still overlap)
constexpr int kLeaderRows = 9;

constexpr float kHalfW = layout::kWidthF * 0.5F;
constexpr float kHalfH = layout::kHeightF * 0.5F;

[[nodiscard]] int difficultyToIndex(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return 0;
    case Difficulty::Hard:
        return 2;
    case Difficulty::Medium:
    case Difficulty::VeryHard:
        break; // Snake offers three difficulties; VeryHard maps to Medium
    }
    return 1;
}

[[nodiscard]] int& snakeBestField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.snakeBestEasy;
    case Difficulty::Hard:
        return s.snakeBestHard;
    case Difficulty::Medium:
    case Difficulty::VeryHard:
        break;
    }
    return s.snakeBestMedium;
}

[[nodiscard]] int quantize10(int value) {
    return (value / 10) * 10;
}

} // namespace

SnakeScene::SnakeScene(SceneManager& manager, Difficulty difficulty)
    : manager_(manager), difficulty_(difficulty),
      world_(difficultyToIndex(difficulty), std::random_device{}()),
      ghost_(std::random_device{}(), kGhostCount), camX_(world_.player().head.x),
      camY_(world_.player().head.y),
      backButton_(IconButton::Icon::Chevron, kBackCx, kBackCy, kBackRadius),
      bestScore_(snakeBestField(settings(), difficulty)),
      overlay_(color(difficulty_), colors::white, kButtonRowY) {
    backButton_.setOnTap([this] { manager_.pop(); });
    overlay_.setOnHome([this] { manager_.popToRoot(); });
    overlay_.setActionLabel("RETRY");
    overlay_.setOnAction(
        [this] { manager_.replace(std::make_unique<SnakeScene>(manager_, difficulty_)); });
}

// ---- Input ------------------------------------------------------------------

bool SnakeScene::handleBoostButton(const PointerEvent& event) {
    const float dx = event.x - kBoostCx;
    const float dy = event.y - kBoostCy;
    const bool inside = (dx * dx) + (dy * dy) <= kBoostR * kBoostR;
    switch (event.phase) {
    case PointerEvent::Phase::Down:
        if (inside) {
            boosting_ = true;
            boostLatched_ = true;
            return true;
        }
        return false;
    case PointerEvent::Phase::Up:
        boosting_ = false;
        if (boostLatched_) {
            boostLatched_ = false;
            return true;
        }
        return false;
    case PointerEvent::Phase::Move:
        // While a finger holds boost it can't also steer (single pointer), so
        // swallow its drags rather than letting them re-aim the head.
        return boostLatched_;
    }
    return false;
}

void SnakeScene::handleSteer(const PointerEvent& event) {
    if (event.phase == PointerEvent::Phase::Up) {
        return; // the head keeps its last aim after the finger lifts
    }
    aimScreen_ = {event.x, event.y};
    hasAim_ = true;
}

void SnakeScene::handleInput(const PointerEvent& event) {
    if (backButton_.handleInput(event)) {
        return;
    }
    if (phase_ == Phase::GameOver) {
        overlay_.handleInput(event);
        return;
    }
    if (handleBoostButton(event)) {
        return;
    }
    handleSteer(event);
}

// ---- Update -----------------------------------------------------------------

void SnakeScene::update(float dtSeconds) {
    if (phase_ == Phase::Playing) {
        const snake::Snake& p = world_.player();
        const Vec2 aimWorld = hasAim_ ? screenToWorld(aimScreen_.x, aimScreen_.y)
                                      : p.head + (snake::fromAngle(p.heading) * 100.0F);
        world_.setPlayerInput({.aimWorld = aimWorld, .boost = boosting_});

        advanceFixed(accum_, dtSeconds, cfg::kFixedDt, cfg::kMaxAccumDt, [this] { world_.step(); });
        if (!world_.playerAlive()) {
            enterGameOver();
        }
    }
    ghost_.advance(dtSeconds);
    updateCamera(dtSeconds);
}

void SnakeScene::enterGameOver() {
    if (recorded_) {
        return;
    }
    phase_ = Phase::GameOver;
    finalScore_ = world_.playerScore();
    bestScore_ = std::max(bestScore_, finalScore_);
    Settings& s = settings();
    snakeBestField(s, difficulty_) = bestScore_;
    saveSettings(s);
    recorded_ = true;
}

void SnakeScene::updateCamera(float dtSeconds) {
    const snake::Snake& p = world_.player();
    const float follow = 1.0F - std::exp(-kFollowRate * dtSeconds);
    camX_ += (p.head.x - camX_) * follow;
    camY_ += (p.head.y - camY_) * follow;
    const float targetZoom =
        std::clamp(kBaseZoom * std::pow(cfg::kBaseRadius / std::max(p.radius, 1.0F), kZoomExp),
                   kMinZoom, kMaxZoom);
    const float ease = 1.0F - std::exp(-kZoomRate * dtSeconds);
    zoom_ += (targetZoom - zoom_) * ease;
}

// ---- Camera transform -------------------------------------------------------

Vec2 SnakeScene::screenToWorld(float sx, float sy) const {
    return {((sx - kHalfW) / zoom_) + camX_, ((sy - kHalfH) / zoom_) + camY_};
}

SnakeScene::ScreenPos SnakeScene::toScreen(Vec2 world) const {
    return {.x = ((world.x - camX_) * zoom_) + kHalfW, .y = ((world.y - camY_) * zoom_) + kHalfH};
}

bool SnakeScene::onScreen(float sx, float sy, float screenRadius) {
    return sx + screenRadius >= 0.0F && sx - screenRadius <= layout::kWidthF &&
           sy + screenRadius >= 0.0F && sy - screenRadius <= layout::kHeightF;
}

// ---- Rendering --------------------------------------------------------------

void SnakeScene::drawArena(Canvas& canvas) const {
    // Faint dotted texture for the void (sparse grid within the visible patch).
    constexpr float kStep = 240.0F;
    const float left = camX_ - (kHalfW / zoom_);
    const float top = camY_ - (kHalfH / zoom_);
    const float right = camX_ + (kHalfW / zoom_);
    const float bottom = camY_ + (kHalfH / zoom_);
    const float startX = std::max(0.0F, std::floor(left / kStep) * kStep);
    const float startY = std::max(0.0F, std::floor(top / kStep) * kStep);
    const float endX = std::min(right, world_.worldSize());
    const float endY = std::min(bottom, world_.worldSize());
    const Color dot = rgb(255, 255, 255, 12);
    const int cols = startX <= endX ? static_cast<int>((endX - startX) / kStep) + 1 : 0;
    const int rows = startY <= endY ? static_cast<int>((endY - startY) / kStep) + 1 : 0;
    for (int ix = 0; ix < cols; ++ix) {
        for (int iy = 0; iy < rows; ++iy) {
            const ScreenPos sp = toScreen({startX + (static_cast<float>(ix) * kStep),
                                           startY + (static_cast<float>(iy) * kStep)});
            canvas.fillCircle(sp.x, sp.y, 3.0F, dot);
        }
    }

    // Arena border (the death edge).
    const ScreenPos a = toScreen({0.0F, 0.0F});
    const ScreenPos b = toScreen({world_.worldSize(), 0.0F});
    const ScreenPos c = toScreen({world_.worldSize(), world_.worldSize()});
    const ScreenPos d = toScreen({0.0F, world_.worldSize()});
    const float thick = std::max(4.0F, 10.0F * zoom_);
    const Color border = theme().snakeBorder;
    canvas.line(a.x, a.y, b.x, b.y, thick, border);
    canvas.line(b.x, b.y, c.x, c.y, thick, border);
    canvas.line(c.x, c.y, d.x, d.y, thick, border);
    canvas.line(d.x, d.y, a.x, a.y, thick, border);
}

void SnakeScene::drawFood(Canvas& canvas) const {
    for (const snake::FoodOrb& orb : world_.food()) {
        const ScreenPos sp = toScreen(orb.pos);
        const float r = std::max(2.0F, orb.radius * zoom_);
        if (!onScreen(sp.x, sp.y, r)) {
            continue;
        }
        canvas.fillCircle(sp.x, sp.y, r, snake::palette::foodColor(orb.colorIndex));
    }
}

void SnakeScene::drawSnake(Canvas& canvas, const snake::Snake& s, bool isPlayer) const {
    const auto segs = static_cast<int>(s.path.size());
    const float invSeg = segs > 1 ? 1.0F / static_cast<float>(segs - 1) : 0.0F;
    const float r = std::max(2.0F, s.radius * zoom_);

    // Body, tail -> head so the head-most disc ends up on top.
    for (int i = segs - 1; i >= 0; i -= kDrawStride) {
        const ScreenPos sp = toScreen(s.path.at(static_cast<std::size_t>(i)));
        if (!onScreen(sp.x, sp.y, r)) {
            continue;
        }
        const float t = static_cast<float>(i) * invSeg;
        canvas.fillCircle(sp.x, sp.y, r, snake::palette::sampleGradient(s.gradIndex, t));
    }

    const ScreenPos hp = toScreen(s.head);
    if (!onScreen(hp.x, hp.y, r)) {
        return;
    }
    canvas.fillCircle(hp.x, hp.y, r, snake::palette::sampleGradient(s.gradIndex, 0.0F));

    // Eyes (only when the head is big enough on screen to read).
    if (r > 9.0F) {
        const Vec2 fwd = snake::fromAngle(s.heading);
        const Vec2 side{-fwd.y, fwd.x};
        const float white = r * 0.42F;
        for (const float sign : {-1.0F, 1.0F}) {
            const float ex = hp.x + (((fwd.x * 0.30F) + (side.x * sign * 0.42F)) * r);
            const float ey = hp.y + (((fwd.y * 0.30F) + (side.y * sign * 0.42F)) * r);
            canvas.fillCircle(ex, ey, white, colors::white);
            canvas.fillCircle(ex + (fwd.x * white * 0.35F), ey + (fwd.y * white * 0.35F),
                              white * 0.5F, rgb(24, 26, 32));
        }
    }

    const std::string label = isPlayer ? "YOU" : s.name;
    const Color nameColor = isPlayer ? colors::white : rgb(214, 218, 228);
    canvas.textCentered(label, hp.x, hp.y + r + 14.0F, 22.0F, nameColor);
}

void SnakeScene::drawHud(Canvas& canvas) const {
    backButton_.render(canvas);
    const float cx = layout::kWidthF * 0.5F;
    canvas.textCentered(label(difficulty_), cx, 42.0F, 26.0F, color(difficulty_));
    canvas.textCentered(std::to_string(world_.playerScore()), cx, 100.0F, 64.0F, colors::white);
}

void SnakeScene::drawLeaderboard(Canvas& canvas) const {
    std::vector<snake::RealEntry> reals;
    const auto& snakes = world_.snakes();
    reals.reserve(snakes.size());
    for (std::size_t i = 1; i < snakes.size(); ++i) {
        const snake::Snake& bot = snakes.at(i);
        if (bot.alive) {
            reals.push_back({.name = bot.name, .score = static_cast<int>(bot.score)});
        }
    }
    const snake::LeaderboardView view =
        ghost_.build(kLeaderRows, "YOU", world_.playerScore(), reals);

    constexpr float kFs = 22.0F;
    constexpr float kRowH = 30.0F;
    const float leftX = layout::kWidthF - 258.0F;
    const float rightX = layout::kWidthF - 16.0F;
    float y = 122.0F;
    for (std::size_t i = 0; i < view.top.size(); ++i) {
        const snake::LeaderRow& row = view.top.at(i);
        const Color c =
            row.isPlayer ? colors::white : snake::palette::foodColor(static_cast<std::uint8_t>(i));
        const std::string left = std::to_string(i + 1) + " " + row.name;
        canvas.text(left, leftX, y, kFs, c, Canvas::Align::Left);
        canvas.text(std::to_string(quantize10(row.score)), rightX, y, kFs, c, Canvas::Align::Right);
        y += kRowH;
    }
    // The player's own pinned row, highlighted.
    y += 10.0F;
    const std::string playerLeft = std::to_string(view.playerRank) + " YOU";
    canvas.text(playerLeft, leftX, y, kFs, colors::botCyan, Canvas::Align::Left);
    canvas.text(std::to_string(view.playerScore), rightX, y, kFs, colors::botCyan,
                Canvas::Align::Right);
}

void SnakeScene::drawBoostButton(Canvas& canvas) const {
    const Color fill = boosting_ ? rgb(250, 205, 70, 110) : rgb(255, 255, 255, 30);
    const Color ring = boosting_ ? rgb(255, 232, 120, 220) : rgb(255, 255, 255, 70);
    canvas.fillCircle(kBoostCx, kBoostCy, kBoostR, ring);
    canvas.fillCircle(kBoostCx, kBoostCy, kBoostR - 6.0F, fill);
    canvas.emojiCentered(kBolt, kBoostCx, kBoostCy, kBoostR * 0.95F);
}

void SnakeScene::drawOverlay(Canvas& canvas) const {
    overlay_.render(canvas, "GAME OVER", 520.0F, 88.0F);
    canvas.textCentered("SCORE  " + std::to_string(finalScore_), layout::kWidthF / 2.0F, 632.0F,
                        44.0F, colors::white);
    canvas.textCentered("BEST  " + std::to_string(bestScore_), layout::kWidthF / 2.0F, 692.0F,
                        34.0F, colors::botCyan);
}

void SnakeScene::render(Canvas& canvas) {
    canvas.clear(theme().snakeField);
    drawArena(canvas);
    drawFood(canvas);

    const auto& snakes = world_.snakes();
    for (std::size_t i = 1; i < snakes.size(); ++i) {
        const snake::Snake& bot = snakes.at(i);
        if (bot.alive) {
            drawSnake(canvas, bot, false);
        }
    }
    drawSnake(canvas, world_.player(), true); // player on top (drawn even when dead)

    drawHud(canvas);
    drawLeaderboard(canvas);
    if (phase_ == Phase::Playing) {
        drawBoostButton(canvas);
    } else {
        drawOverlay(canvas);
    }
}

} // namespace og
