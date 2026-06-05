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

// A rounded "pill" of text: a filled rounded rect sized to the label with the
// text centered on it. Used for avatar name tags and the leaderboard header.
void drawTextPill(Canvas& canvas, std::string_view text, float cx, float cy, float fs, Color fill,
                  Color textColor, float padX, float padY) {
    const Canvas::Size sz = canvas.measure(text, fs);
    const float w = sz.w + (padX * 2.0F);
    const float h = sz.h + (padY * 2.0F);
    canvas.fillRoundedRect(cx - (w * 0.5F), cy - (h * 0.5F), w, h, h * 0.5F, fill);
    canvas.textCentered(text, cx, cy, fs, textColor);
}

// Translucent dark panel backing for the HUD / leaderboard / minimap chrome.
constexpr Color kPanelBg = rgb(16, 18, 24, 222);
constexpr Color kPanelInk = rgb(214, 218, 228);    // light text on the dark panels
constexpr Color kHeaderEdge = rgb(60, 200, 222);   // cyan header outline (matches the reference)
constexpr const char* kCrown = "\xF0\x9F\x91\x91"; // 👑

// ---- Cut-through spark FX (cosmetic) ----------------------------------------
namespace fx {
constexpr float kSpawnInterval = 0.016F; // seconds between spark bursts while cutting
constexpr int kPerBurst = 3;             // sparks per cutting on-screen player, per burst
constexpr std::size_t kMaxParticles = 200;
constexpr float kLifeMin = 0.30F;
constexpr float kLifeMax = 0.58F;
constexpr float kSpeedMin = 30.0F; // outward drift, world units/sec
constexpr float kSpeedMax = 110.0F;
constexpr float kLiftVelMin = 60.0F; // initial pop upward
constexpr float kLiftVelMax = 150.0F;
constexpr float kGravity = 300.0F;  // pulls the lift back toward the ground
constexpr float kSizeMin = 4.0F;    // square half-extent is size*0.5, in logical px @ zoom 1
constexpr float kSizeMax = 9.0F;
constexpr float kSpawnJitter = 14.0F; // scatter around the avatar, world units
constexpr float kOnScreenMargin = 60.0F;
constexpr float kFadeExp = 0.6F; // <1 holds the spark bright then drops off late
} // namespace fx

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
    return p.pos; // free movement: the avatar's continuous world position
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
    animTime_ += dtSeconds; // drives cosmetic-only laser/gem animation
    if (phase_ == Phase::Playing) {
        if (hasAim_) {
            const Vec2 aim = screenToWorld(aimScreen_.x, aimScreen_.y);
            const Vec2 dir = aim - avatarWorld(world_.player());
            if (hexanaut::length(dir) > 4.0F) {
                // Free movement: steer toward wherever the finger points (the sim
                // caps how fast the avatar can curve to it).
                world_.setPlayerDesiredAngle(hexanaut::angleOf(dir));
            }
        }
        accum_ += std::min(dtSeconds, cfg::kMaxAccumDt);
        while (accum_ >= cfg::kFixedDt) {
            world_.step();
            accum_ -= cfg::kFixedDt;
        }
        spawnCutFx(dtSeconds);
        if (!world_.playerAlive()) {
            enterGameOver();
        }
    }
    updateLasers(dtSeconds);    // spawn/age shooter bolts (keep fading after game over)
    updateParticles(dtSeconds); // keep advancing/fading even after game over
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

void HexanautScene::appendHexTop(Vec2 centerWorld, float inset, float lift, Color colorTop,
                                 Color colorBottom) {
    // Blend colorTop (north corners) -> colorBottom (south corners) by a corner's
    // vertical position so each face picks up a subtle top-lit bevel.
    constexpr float kInvSpan = 1.0F / std::numbers::sqrt3_v<float>; // corner uy in [-.866,.866]
    const auto base = static_cast<int>(meshVerts_.size());
    const ScreenPos cs = toScreen(centerWorld, lift);
    meshVerts_.push_back({.x = cs.x, .y = cs.y, .color = pal::mix(colorTop, colorBottom, 0.5F)});
    for (int k = 0; k < 6; ++k) {
        const Vec2 unit = cornerUnits().at(static_cast<std::size_t>(k));
        const float t = std::clamp((unit.y * kInvSpan * 0.5F) + 0.5F, 0.0F, 1.0F);
        const ScreenPos sp = toScreen(centerWorld + cornerOffset(k, inset), lift);
        meshVerts_.push_back({.x = sp.x, .y = sp.y, .color = pal::mix(colorTop, colorBottom, t)});
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
    trailOutlines_.clear();

    const hexanaut::HexGrid& grid = world_.grid();
    constexpr float kS = cfg::kHexSize;
    const float worldMargin = (2.0F * kS) + cfg::kTrailLift;
    const float wxMin = camX_ - (kHalfW / zoom_) - worldMargin;
    const float wxMax = camX_ + (kHalfW / zoom_) + worldMargin;
    const float wyMin = camY_ - (kHalfH / (cfg::kSquash * zoom_)) - worldMargin;
    const float wyMax = camY_ + (kHalfH / (cfg::kSquash * zoom_)) + worldMargin;

    // Visible axial range, NOT clamped to the grid: cells beyond the board get a
    // faint void honeycomb so the screen is always full (no hard black at edges).
    const float invQ = 1.0F / (1.5F * kS);
    const float invR = 1.0F / (std::numbers::sqrt3_v<float> * kS);
    const int qMin = static_cast<int>(std::floor(wxMin * invQ)) - 1;
    const int qMax = static_cast<int>(std::ceil(wxMax * invQ)) + 1;
    const int rMin =
        static_cast<int>(std::floor((wyMin * invR) - (static_cast<float>(qMax) * 0.5F))) - 1;
    const int rMax =
        static_cast<int>(std::ceil((wyMax * invR) - (static_cast<float>(qMin) * 0.5F))) + 1;

    const float cullMargin = (2.0F * kS * zoom_) + (cfg::kTrailLift * zoom_);

    // Back-to-front: ascending r (then q) so nearer rows overlap farther ones.
    for (int r = rMin; r <= rMax; ++r) {
        for (int q = qMin; q <= qMax; ++q) {
            const HexCoord coord{q, r};
            const Vec2 center = hexanaut::axialToWorld(coord, kS);
            const ScreenPos cs = toScreen(center, 0.0F);
            if (cs.x < -cullMargin || cs.x > layout::kWidthF + cullMargin || cs.y < -cullMargin ||
                cs.y > layout::kHeightF + cullMargin) {
                continue;
            }
            if (grid.contains(coord)) {
                appendCellPrism(grid, coord, center);
            } else {
                appendHexTop(center, cfg::kGroundInset, 0.0F, pal::kVoid, pal::kVoid);
            }
        }
    }

    canvas.fillMesh(meshVerts_, meshIdx_);
}

void HexanautScene::appendOwnedPrism(const hexanaut::HexGrid& grid, HexCoord coord, Vec2 center,
                                     hexanaut::PlayerId owner) {
    // Extruded territory block: front-facing walls only where it borders a cell of a
    // *different* owner (open ground/neutral counts). Same-owner neighbors stay
    // flush — including ones a rival is cutting a trail across — so no internal walls
    // line the cut; the attacker's claim shows as an outline on top instead.
    const Color base = pal::topColor(owner);
    const Color wallTop = pal::darken(base, pal::kWallTop);
    const Color wallBottom = pal::darken(base, pal::kWallBottom);
    for (int e = 0; e <= 2; ++e) {
        const HexCoord nb = hexanaut::neighbor(coord, kEdgeToDir.at(static_cast<std::size_t>(e)));
        if (!grid.contains(nb) || grid.at(nb).owner != owner) {
            appendWall(center, e, cfg::kPrismLift, wallTop, wallBottom);
        }
    }
    appendHexTop(center, 1.04F, cfg::kPrismLift, pal::shade(base, pal::kFaceTop),
                 pal::shade(base, pal::kFaceBottom));
}

void HexanautScene::appendCellPrism(const hexanaut::HexGrid& grid, HexCoord coord, Vec2 center) {
    const Cell& cell = grid.at(coord);
    if (cell.trailOwner != hexanaut::kNoTrail) {
        if (cell.owner != hexanaut::kNeutral) {
            // Cutting across someone else's land: leave their territory block intact
            // (fill + height) and stroke OUR bright outline on its top face. The enemy
            // block stays visible with the attacker's pending claim marked over it —
            // they only lose it once the loop closes (closeTrailAndCapture).
            appendOwnedPrism(grid, coord, center, cell.owner);
            trailOutlines_.push_back(
                {.center = center, .owner = cell.trailOwner, .lift = cfg::kPrismLift});
        } else {
            // Out-of-territory trail over open ground: the cell stays at ground level,
            // faintly tinted toward the owner's color, with a bright hex outline on top
            // (the claimed-but-not-captured look). Closing the loop extrudes it.
            const Color owner = pal::topColor(cell.trailOwner);
            appendHexTop(center, cfg::kGroundInset, 0.0F, pal::mix(pal::kGroundTop, owner, 0.22F),
                         pal::mix(pal::kGroundBottom, owner, 0.16F));
            trailOutlines_.push_back({.center = center, .owner = cell.trailOwner, .lift = 0.0F});
        }
    } else if (cell.owner != hexanaut::kNeutral) {
        appendOwnedPrism(grid, coord, center, cell.owner);
    } else {
        appendHexTop(center, cfg::kGroundInset, 0.0F, pal::kGroundTop, pal::kGroundBottom);
    }
    // Speed/Vision render as floating bubbles here; the Shooter is drawn separately
    // (crystal + laser) from the sim's shooter list, so skip it in this pass.
    if (cell.powerup != 0 &&
        cell.powerup != static_cast<std::uint8_t>(hexanaut::PowerUp::Shooter)) {
        powerupDraws_.push_back({.center = center, .type = cell.powerup});
    }
}

void HexanautScene::drawTrailOutlines(Canvas& canvas) const {
    // Stroke a bright owner-colored border around each active-trail hex (collected
    // during drawField). Drawn on top of the field mesh so the outline reads
    // cleanly over the faintly-tinted ground beneath it.
    const float thick = std::max(2.0F, cfg::kHexSize * zoom_ * 0.11F);
    for (const TrailOutline& t : trailOutlines_) {
        const Color col = pal::lighten(pal::topColor(t.owner), 0.30F);
        std::array<ScreenPos, 6> corners{};
        for (int k = 0; k < 6; ++k) {
            corners.at(static_cast<std::size_t>(k)) =
                toScreen(t.center + cornerOffset(k, cfg::kGroundInset), t.lift);
        }
        for (int k = 0; k < 6; ++k) {
            const ScreenPos& a = corners.at(static_cast<std::size_t>(k));
            const ScreenPos& b = corners.at(static_cast<std::size_t>((k + 1) % 6));
            canvas.line(a.x, a.y, b.x, b.y, thick, col);
        }
    }
}

void HexanautScene::drawTrails(Canvas& canvas) {
    // A raised, glossy "rope" tube behind every player that is currently outside
    // its territory (i.e. has an active trail), riding above the flat outlined
    // trail cells. The path is the ordered trail cell centers plus the live head;
    // it's stroked in three passes — a dark rim, the body, and a top highlight —
    // with discs at each joint so the bends stay round (Canvas::line is butt-cap).
    constexpr float kS = cfg::kHexSize;
    const float radius = std::max(4.0F, kS * zoom_ * cfg::kTrailRopeRadius);
    for (const Player& p : world_.players()) {
        if (!p.alive || p.trail.empty()) {
            continue;
        }
        const Color owner = pal::topColor(p.id);
        const Color rim = pal::darken(owner, 0.45F);
        const Color gloss = pal::lighten(owner, 0.45F);

        ropeScratch_.clear();
        for (const HexCoord& c : p.trail) {
            ropeScratch_.push_back(toScreen(hexanaut::axialToWorld(c, kS), cfg::kTrailRopeLift));
        }
        ropeScratch_.push_back(toScreen(p.pos, cfg::kTrailRopeLift));

        // Routing through hex-cell centers leaves a sawtooth on diagonal runs; two
        // 3-tap binomial passes cancel that Nyquist zig-zag for a smooth rope while
        // keeping the endpoints (territory anchor and live head) pinned.
        for (int pass = 0; pass < 2; ++pass) {
            ScreenPos prev = ropeScratch_.front();
            for (std::size_t i = 1; i + 1 < ropeScratch_.size(); ++i) {
                const ScreenPos cur = ropeScratch_.at(i);
                const ScreenPos& nxt = ropeScratch_.at(i + 1);
                ropeScratch_.at(i) = {.x = (prev.x * 0.25F) + (cur.x * 0.5F) + (nxt.x * 0.25F),
                                      .y = (prev.y * 0.25F) + (cur.y * 0.5F) + (nxt.y * 0.25F)};
                prev = cur; // original (un-smoothed) neighbor for the next step
            }
        }

        const auto stroke = [&](float rad, Color col, float dy) {
            for (std::size_t i = 0; i + 1 < ropeScratch_.size(); ++i) {
                const ScreenPos& a = ropeScratch_.at(i);
                const ScreenPos& b = ropeScratch_.at(i + 1);
                canvas.line(a.x, a.y + dy, b.x, b.y + dy, rad * 2.0F, col);
            }
            for (const ScreenPos& s : ropeScratch_) {
                canvas.fillCircle(s.x, s.y + dy, rad, col);
            }
        };
        stroke(radius + 2.5F, rim, 0.0F);               // dark rim → rounded 3D edge
        stroke(radius, owner, 0.0F);                    // tube body
        stroke(radius * 0.42F, gloss, -radius * 0.42F); // glossy top highlight
    }
}

void HexanautScene::spawnCutFx(float dtSeconds) {
    fxSpawnAccum_ += dtSeconds;
    if (fxSpawnAccum_ < fx::kSpawnInterval) {
        return;
    }
    const hexanaut::HexGrid& grid = world_.grid();
    std::uniform_real_distribution<float> u(0.0F, 1.0F);
    const auto lerpf = [](float a, float b, float t) { return a + ((b - a) * t); };
    constexpr float kTwoPi = 2.0F * std::numbers::pi_v<float>;

    while (fxSpawnAccum_ >= fx::kSpawnInterval) {
        fxSpawnAccum_ -= fx::kSpawnInterval;
        for (const Player& p : world_.players()) {
            if (!p.alive || !grid.contains(p.cell)) {
                continue;
            }
            // "Cutting" = sitting over a cell still owned by someone else.
            const Cell& under = grid.at(p.cell);
            if (under.owner == hexanaut::kNeutral || under.owner == p.id) {
                continue;
            }
            const ScreenPos sp = toScreen(p.pos, 0.0F);
            if (sp.x < -fx::kOnScreenMargin || sp.x > layout::kWidthF + fx::kOnScreenMargin ||
                sp.y < -fx::kOnScreenMargin || sp.y > layout::kHeightF + fx::kOnScreenMargin) {
                continue; // off-screen player: nothing to show, don't burn the pool
            }
            const Color spark = pal::lighten(pal::topColor(p.id), 0.85F);
            for (int k = 0; k < fx::kPerBurst && particles_.size() < fx::kMaxParticles; ++k) {
                const float a = u(fxRng_) * kTwoPi;
                const float r = u(fxRng_) * fx::kSpawnJitter;
                const float va = u(fxRng_) * kTwoPi;
                const float vspd = lerpf(fx::kSpeedMin, fx::kSpeedMax, u(fxRng_));
                particles_.push_back(
                    Particle{.pos = {p.pos.x + (std::cos(a) * r), p.pos.y + (std::sin(a) * r)},
                             .vel = {std::cos(va) * vspd, std::sin(va) * vspd},
                             .lift = 0.0F,
                             .liftVel = lerpf(fx::kLiftVelMin, fx::kLiftVelMax, u(fxRng_)),
                             .age = 0.0F,
                             .life = lerpf(fx::kLifeMin, fx::kLifeMax, u(fxRng_)),
                             .size = lerpf(fx::kSizeMin, fx::kSizeMax, u(fxRng_)),
                             .color = spark});
            }
        }
    }
}

void HexanautScene::updateParticles(float dtSeconds) {
    for (Particle& p : particles_) {
        p.age += dtSeconds;
        p.pos = p.pos + (p.vel * dtSeconds);
        p.liftVel -= fx::kGravity * dtSeconds;
        p.lift += p.liftVel * dtSeconds;
        if (p.lift < 0.0F) {
            p.lift = 0.0F; // settle on the ground and rest there for the rest of its life
            p.liftVel = 0.0F;
        }
    }
    std::erase_if(particles_, [](const Particle& p) { return p.age >= p.life; });
}

void HexanautScene::drawParticles(Canvas& canvas) {
    if (particles_.empty()) {
        return;
    }
    // One mesh for every live spark (the field/minimap buffers are free here).
    meshVerts_.clear();
    meshIdx_.clear();
    for (const Particle& p : particles_) {
        const float t = std::clamp(p.age / p.life, 0.0F, 1.0F);
        const float alpha = std::pow(1.0F - t, fx::kFadeExp); // hold bright, drop off late
        const ScreenPos sp = toScreen(p.pos, p.lift);
        const float half = p.size * 0.5F * zoom_;
        Color c = p.color;
        c.a = static_cast<std::uint8_t>(alpha * 255.0F);
        const auto base = static_cast<int>(meshVerts_.size());
        meshVerts_.push_back({.x = sp.x - half, .y = sp.y - half, .color = c});
        meshVerts_.push_back({.x = sp.x + half, .y = sp.y - half, .color = c});
        meshVerts_.push_back({.x = sp.x + half, .y = sp.y + half, .color = c});
        meshVerts_.push_back({.x = sp.x - half, .y = sp.y + half, .color = c});
        for (const int i : {0, 1, 2, 0, 2, 3}) {
            meshIdx_.push_back(base + i);
        }
    }
    canvas.fillMesh(meshVerts_, meshIdx_);
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

void HexanautScene::updateLasers(float dtSeconds) {
    constexpr std::size_t kMaxLasers = 48; // safety cap; a handful are live at once
    const std::vector<hexanaut::Shooter>& shooters = world_.shooters();
    if (shotSeen_.size() < shooters.size()) {
        shotSeen_.resize(shooters.size(), 0); // new shooters start un-fired (shotCount 0)
    }
    for (std::size_t i = 0; i < shooters.size(); ++i) {
        const hexanaut::Shooter& s = shooters.at(i);
        if (s.shotCount == shotSeen_.at(i)) {
            continue; // no new capture since last frame
        }
        shotSeen_.at(i) = s.shotCount;
        const hexanaut::PlayerId owner = world_.ownerAt(s.cell);
        if (owner != hexanaut::kNeutral && lasers_.size() < kMaxLasers) {
            lasers_.push_back({.from = hexanaut::axialToWorld(s.cell, cfg::kHexSize),
                               .to = hexanaut::axialToWorld(s.target, cfg::kHexSize),
                               .owner = owner,
                               .age = 0.0F,
                               .life = cfg::kShooterLaserLife});
        }
    }
    for (Laser& l : lasers_) {
        l.age += dtSeconds;
    }
    std::erase_if(lasers_, [](const Laser& l) { return l.age >= l.life; });
}

void HexanautScene::drawShooters(Canvas& canvas) const {
    constexpr float kS = cfg::kHexSize;

    // ---- Fired laser bolts: each fades (lerps) to nothing over its life ---------
    for (const Laser& l : lasers_) {
        const float fade = std::clamp(1.0F - (l.age / l.life), 0.0F, 1.0F);
        const Color col = pal::topColor(l.owner);
        const ScreenPos a = toScreen(l.from, cfg::kPrismLift + 22.0F); // muzzle (the gem)
        const ScreenPos b = toScreen(l.to, cfg::kPrismLift + 4.0F);    // the cell it hit
        const float beamW = std::max(3.0F, kS * zoom_ * 0.2F);
        const auto alpha = [&](float base) { return static_cast<std::uint8_t>(base * fade); };
        canvas.line(a.x, a.y, b.x, b.y, beamW * 2.4F, pal::withAlpha(col, alpha(70.0F)));  // glow
        canvas.line(a.x, a.y, b.x, b.y, beamW, pal::withAlpha(col, alpha(200.0F)));        // body
        canvas.line(a.x, a.y, b.x, b.y, std::max(1.5F, beamW * 0.4F),
                    pal::withAlpha(pal::lighten(col, 0.6F), alpha(240.0F)));               // hot core
        // Impact flash blooms outward a touch as it dies.
        const float flashR = beamW * (1.5F + ((1.0F - fade) * 1.4F));
        canvas.fillCircle(b.x, b.y, flashR, pal::withAlpha(pal::lighten(col, 0.3F), alpha(190.0F)));
        canvas.fillCircle(b.x, b.y, flashR * 0.45F, pal::withAlpha(colors::white, alpha(200.0F)));
    }

    // ---- Crystal tokens ---------------------------------------------------------
    for (const hexanaut::Shooter& s : world_.shooters()) {
        const Vec2 cellW = hexanaut::axialToWorld(s.cell, kS);
        const hexanaut::PlayerId owner = world_.ownerAt(s.cell);
        const bool active = owner != hexanaut::kNeutral;
        const Color col = active ? pal::topColor(owner) : rgb(120, 126, 140);

        // Crystal floats above its cell with a gentle, per-cell-phased bob.
        const float bob =
            std::sin((animTime_ * 3.0F) + (static_cast<float>(s.cell.q) * 0.7F)) * 3.0F;
        const ScreenPos gp = toScreen(cellW, cfg::kPrismLift + 20.0F + bob);
        const float rad = std::max(11.0F, kS * zoom_ * 0.5F);

        // ---- Crystal token ------------------------------------------------------
        const ScreenPos shadow = toScreen(cellW, cfg::kPrismLift);
        canvas.fillCircle(shadow.x, shadow.y + (rad * 0.2F), rad * 0.85F, rgb(0, 0, 0, 80));
        if (active) {
            canvas.fillCircle(gp.x, gp.y, rad * 1.5F, pal::withAlpha(col, 45)); // glow halo
        }
        // Faceted gem: a 4-corner diamond gradient-shaded top(lit)->bottom(shadow),
        // tinted to the owner color (grey when nobody owns it).
        const Color top = active ? pal::lighten(col, 0.45F) : rgb(150, 156, 168);
        const Color bottom = active ? pal::darken(col, 0.45F) : rgb(72, 76, 86);
        const Color mid = pal::mix(top, bottom, 0.5F);
        const std::array<Canvas::Vertex, 4> gem{{
            {.x = gp.x, .y = gp.y - rad, .color = top},
            {.x = gp.x + (rad * 0.72F), .y = gp.y, .color = mid},
            {.x = gp.x, .y = gp.y + (rad * 0.85F), .color = bottom},
            {.x = gp.x - (rad * 0.72F), .y = gp.y, .color = mid},
        }};
        canvas.fillConvexPolygon(gem);
        // Bright rim edges + a glowing core facet.
        const Color rim = pal::withAlpha(active ? pal::lighten(col, 0.25F) : rgb(196, 200, 210), 205);
        const float rimW = std::max(1.5F, kS * zoom_ * 0.05F);
        for (std::size_t k = 0; k < 4; ++k) {
            const Canvas::Vertex& a = gem.at(k);
            const Canvas::Vertex& b = gem.at((k + 1) % 4);
            canvas.line(a.x, a.y, b.x, b.y, rimW, rim);
        }
        canvas.fillCircle(gp.x, gp.y - (rad * 0.05F), rad * 0.26F,
                          pal::withAlpha(pal::lighten(col, 0.55F), active ? 235 : 150));
    }
}

void HexanautScene::drawAvatars(Canvas& canvas) const {
    for (const Player& p : world_.players()) {
        if (!p.alive) {
            continue;
        }
        const Vec2 pw = avatarWorld(p);
        // Sit on the surface the avatar is actually over: riding the raised rope
        // while laying a trail, on top of its own (extruded) territory at home,
        // else flat on the ground.
        const hexanaut::HexGrid& grid = world_.grid();
        float surfaceLift = 0.0F;
        if (!p.trail.empty()) {
            surfaceLift = cfg::kTrailRopeLift;
        } else if (grid.contains(p.cell)) {
            const Cell& under = grid.at(p.cell);
            if (under.trailOwner == hexanaut::kNoTrail && under.owner == p.id) {
                surfaceLift = cfg::kPrismLift;
            }
        }
        const ScreenPos hp = toScreen(pw, surfaceLift + 6.0F);
        const float r = std::max(7.0F, cfg::kHexSize * zoom_ * 0.5F);
        const Color body = pal::topColor(p.id);

        // Heading as a screen-space unit (squash the y the way the projection
        // does) so the visor and highlight sit correctly for the tilted view.
        const Vec2 fwd = hexanaut::unitFromAngle(p.angle);
        float dx = fwd.x;
        float dy = fwd.y * cfg::kSquash;
        const float dl = std::sqrt((dx * dx) + (dy * dy));
        if (dl > 0.0001F) {
            dx /= dl;
            dy /= dl;
        }

        // Soft contact shadow tucked just under the token (same plane as the trail
        // top so it stays attached), then dark rim, lit body, a dark "visor" facing
        // the heading, and a small highlight — reads as a little helmeted token
        // whose facing direction is legible at a glance (replaces the old nose).
        const ScreenPos shadow = toScreen(pw, surfaceLift);
        canvas.fillCircle(shadow.x, shadow.y + (r * 0.35F), r * 0.95F, rgb(0, 0, 0, 90));
        canvas.fillCircle(hp.x, hp.y, r + 3.0F, rgb(16, 18, 24));
        canvas.fillCircle(hp.x, hp.y, r, pal::lighten(body, 0.16F));
        canvas.fillCircle(hp.x + (dx * r * 0.40F), hp.y + (dy * r * 0.40F), r * 0.52F,
                          rgb(26, 28, 36));
        canvas.fillCircle(hp.x - (dx * r * 0.34F), hp.y - (dy * r * 0.34F), r * 0.24F,
                          rgb(255, 255, 255, 205));

        // Name tag on a rounded pill: a darkened tint of the owner's color for the
        // human, a neutral dark chip for bots.
        const Color pill = p.isBot ? rgb(28, 30, 38, 228) : pal::darken(body, 0.62F);
        const Color tagText = p.isBot ? kPanelInk : colors::white;
        drawTextPill(canvas, p.name, hp.x, hp.y - r - 20.0F, 22.0F, pill, tagText, 14.0F, 7.0F);
    }
}

void HexanautScene::drawHud(Canvas& canvas) const {
    drawBackButton(canvas);

    // Your rank among all players (1 = most territory) drives the crown badge.
    const float myPct = world_.playerPercent();
    int rank = 1;
    for (const Player& other : world_.players()) {
        if (world_.percent(other.id) > myPct) {
            ++rank;
        }
    }

    // Left HUD panel: difficulty tag, your territory %, kills, and a crown if 1st.
    constexpr float kPx = 158.0F;
    constexpr float kPy = 44.0F;
    constexpr float kPw = 250.0F;
    constexpr float kPh = 118.0F;
    canvas.fillRoundedRect(kPx, kPy, kPw, kPh, 18.0F, kPanelBg);

    canvas.text(label(difficulty_), kPx + 20.0F, kPy + 13.0F, 22.0F, color(difficulty_),
                Canvas::Align::Left);

    const Color body = pal::topColor(world_.player().id);
    canvas.fillRoundedRect(kPx + 20.0F, kPy + 52.0F, 22.0F, 22.0F, 5.0F, body);
    canvas.text(formatPercent(myPct), kPx + 54.0F, kPy + 42.0F, 44.0F, colors::white,
                Canvas::Align::Left);

    canvas.text("KILLS  " + std::to_string(world_.player().kills), kPx + 20.0F, kPy + 92.0F, 20.0F,
                colors::textMuted, Canvas::Align::Left);
    if (rank == 1) {
        canvas.emojiCentered(kCrown, kPx + kPw - 34.0F, kPy + 60.0F, 36.0F);
    } else {
        canvas.text("#" + std::to_string(rank), kPx + kPw - 24.0F, kPy + 92.0F, 20.0F, kPanelInk,
                    Canvas::Align::Right);
    }
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

    constexpr int kMaxRows = 6;
    constexpr float kFs = 21.0F;
    constexpr float kRowH = 33.0F;
    constexpr float kPad = 12.0F;
    constexpr float kHeaderH = 40.0F;
    constexpr float kPanelW = 292.0F;
    const float panelX = layout::kWidthF - kPanelW - 14.0F;
    constexpr float panelY = 28.0F;

    const int shown = std::min<int>(kMaxRows, static_cast<int>(rows.size()));
    int youIdx = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!rows.at(i).p->isBot) {
            youIdx = static_cast<int>(i);
        }
    }
    const bool youOutside = youIdx >= shown;

    const float rowsTop = panelY + kPad + kHeaderH + 26.0F;
    const float panelH = (rowsTop - panelY) + (static_cast<float>(shown) * kRowH) +
                         (youOutside ? kRowH + 10.0F : 0.0F) + kPad;
    canvas.fillRoundedRect(panelX, panelY, kPanelW, panelH, 16.0F, kPanelBg);

    // Header pill with a cyan outline, like the reference's LEADERBOARD banner.
    const float hx = panelX + kPad;
    const float hy = panelY + kPad;
    const float hw = kPanelW - (2.0F * kPad);
    canvas.fillRoundedRect(hx - 2.0F, hy - 2.0F, hw + 4.0F, kHeaderH + 4.0F, 12.0F, kHeaderEdge);
    canvas.fillRoundedRect(hx, hy, hw, kHeaderH, 10.0F, rgb(26, 28, 36));
    canvas.textCentered("LEADERBOARD", panelX + (kPanelW * 0.5F), hy + (kHeaderH * 0.5F), 24.0F,
                        colors::white);
    canvas.text(std::to_string(aliveCount) + " players", panelX + kPanelW - kPad - 4.0F,
                hy + kHeaderH + 4.0F, 18.0F, colors::textMuted, Canvas::Align::Right);

    const float chipX = panelX + kPad + 6.0F;
    const float nameX = chipX + 28.0F;
    const float pctX = panelX + kPanelW - kPad - 4.0F;
    constexpr float kChip = 18.0F;

    const auto drawRow = [&](int displayRank, const Player& p, float pct, float y, bool you) {
        if (you) {
            canvas.fillRoundedRect(panelX + 6.0F, y - 3.0F, kPanelW - 12.0F, kRowH - 3.0F, 8.0F,
                                   rgb(255, 255, 255, 20));
        }
        const Color chipCol = pal::topColor(p.id);
        canvas.fillRoundedRect(chipX, y + ((kRowH - kChip) * 0.5F) - 3.0F, kChip, kChip, 4.0F,
                               chipCol);
        const Color textCol = you ? pal::lighten(chipCol, 0.25F) : colors::white;
        canvas.text(std::to_string(displayRank) + "  " + p.name, nameX, y, kFs, textCol,
                    Canvas::Align::Left);
        canvas.text(formatPercent(pct), pctX, y, kFs, textCol, Canvas::Align::Right);
    };

    float y = rowsTop;
    for (int i = 0; i < shown; ++i) {
        const Row& row = rows.at(static_cast<std::size_t>(i));
        drawRow(i + 1, *row.p, row.pct, y, !row.p->isBot);
        y += kRowH;
    }
    if (youOutside) {
        y += 4.0F;
        canvas.line(panelX + 12.0F, y - 2.0F, panelX + kPanelW - 12.0F, y - 2.0F, 1.0F,
                    rgb(255, 255, 255, 40));
        y += 6.0F;
        const Row& me = rows.at(static_cast<std::size_t>(youIdx));
        drawRow(youIdx + 1, *me.p, me.pct, y, true);
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

    // Static shooter items: a small dark tile + magenta crystal (the in-game icon),
    // pinned at fixed cells. The border tints to the current owner (grey while
    // un-claimed) so the minimap shows where every item is and who holds it.
    for (const hexanaut::Shooter& s : world_.shooters()) {
        const float ix = boxX + ((static_cast<float>(s.cell.q) + 0.5F) * cw);
        const float iy = boxY + ((static_cast<float>(s.cell.r) + 0.5F) * ch);
        const hexanaut::PlayerId owner = world_.ownerAt(s.cell);
        constexpr float kIcon = 6.0F; // tile half-size
        const Color border =
            owner == hexanaut::kNeutral ? rgb(150, 154, 164) : pal::topColor(owner);
        canvas.fillRoundedRect(ix - kIcon - 1.5F, iy - kIcon - 1.5F, (kIcon + 1.5F) * 2.0F,
                               (kIcon + 1.5F) * 2.0F, 3.0F, border);
        canvas.fillRoundedRect(ix - kIcon, iy - kIcon, kIcon * 2.0F, kIcon * 2.0F, 2.5F,
                               rgb(18, 20, 28));
        const Color gem = rgb(232, 70, 200);
        const std::array<Canvas::Vertex, 4> crystal{{
            {.x = ix, .y = iy - (kIcon * 0.62F), .color = pal::lighten(gem, 0.35F)},
            {.x = ix + (kIcon * 0.5F), .y = iy, .color = gem},
            {.x = ix, .y = iy + (kIcon * 0.62F), .color = pal::darken(gem, 0.4F)},
            {.x = ix - (kIcon * 0.5F), .y = iy, .color = gem},
        }};
        canvas.fillConvexPolygon(crystal);
        canvas.fillCircle(ix, iy, kIcon * 0.17F, pal::lighten(gem, 0.5F)); // core glint
    }

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
    drawTrailOutlines(canvas);
    drawTrails(canvas);
    drawShooters(canvas);
    drawPowerups(canvas);
    drawAvatars(canvas);
    drawParticles(canvas);
    drawHud(canvas);
    drawLeaderboard(canvas);
    drawMinimap(canvas);
    if (phase_ == Phase::GameOver) {
        drawOverlay(canvas);
    }
}

} // namespace og
