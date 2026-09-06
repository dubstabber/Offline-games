#include "games/arrows/ArrowsScene.hpp"

#include "core/Canvas.hpp"
#include "core/Color.hpp"
#include "core/Easing.hpp"
#include "core/GridLayout.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"
#include "games/arrows/ArrowsBoard.hpp"
#include "games/arrows/ArrowsGenerator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace og {
namespace {

// ---- Circular chrome buttons (top corners) ---------------------------------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;
constexpr float kResetCx = layout::kWidthF - 92.0F;
constexpr float kResetCy = 100.0F;
constexpr float kResetRadius = 56.0F;

// ---- Top-bar title + hearts -------------------------------------------------
constexpr float kDiffLabelCy = 70.0F;
constexpr float kLevelLabelCy = 132.0F;
constexpr float kHeartsCy = 208.0F;
constexpr float kHeartSize = 44.0F;
constexpr float kHeartPitch = 76.0F;
constexpr float kHeartPopSeconds = 0.4F;

// ---- Board play area: the grid is fitted/centred into this rect -------------
constexpr float kAreaX = 20.0F;
constexpr float kAreaTop = 252.0F;
constexpr float kAreaBottom = 1380.0F;
constexpr float kAreaW = layout::kWidthF - (2.0F * kAreaX);
constexpr float kAreaH = kAreaBottom - kAreaTop;
constexpr float kMaxCellPx = 96.0F;

// ---- Arrow geometry, in cells ----------------------------------------------
constexpr float kTailExt = 0.25F; // shaft continues this far behind the tail centre
constexpr float kTipExt = 0.36F;  // chevron tip this far ahead of the head centre
constexpr float kWing = 0.34F;    // chevron wing length, back and sideways from the tip
constexpr float kStroke = 0.14F;  // shaft thickness (clamped to a readable pixel range)
constexpr float kMinStrokePx = 5.0F;
constexpr float kMaxStrokePx = 11.0F;
constexpr float kDotRadius = 0.05F;
constexpr float kClipMargin = 0.3F; // how far past the grid a departing arrow may show

// ---- Motion -----------------------------------------------------------------
constexpr float kFlyCellsPerSecond = 22.0F;
constexpr float kBumpCells = 0.3F;    // how far a blocked arrow nudges forward
constexpr float kBumpSeconds = 0.28F; // the nudge out and back
constexpr float kFlashSeconds = 0.7F; // red flash on the tapped arrow and its blocker
constexpr float kTouchReach = 0.8F;   // cells: how far from a tap an arrow may be

// ---- Overlay ----------------------------------------------------------------
constexpr float kButtonRowY = 820.0F;

constexpr const char* kReset = "\xF0\x9F\x94\x84"; // 🔄

// The Settings field holding the current level for a difficulty.
[[nodiscard]] int& savedLevelField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.arrowsLevelEasy;
    case Difficulty::Medium:
        return s.arrowsLevelMedium;
    case Difficulty::Hard:
    case Difficulty::VeryHard:
        return s.arrowsLevelHard;
    }
    return s.arrowsLevelEasy;
}

void setSavedLevel(Difficulty difficulty, int level) {
    savedLevelField(settings(), difficulty) = std::max(1, level);
    saveSettings(settings());
}

[[nodiscard]] float distance(float x0, float y0, float x1, float y1) {
    return std::sqrt(((x1 - x0) * (x1 - x0)) + ((y1 - y0) * (y1 - y0)));
}

// Liang–Barsky: clip the segment to the rectangle in place. Returns false when
// nothing of it lies inside.
bool clipSegment(float rx0, float ry0, float rx1, float ry1, float& x0, float& y0, float& x1,
                 float& y1) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    float t0 = 0.0F;
    float t1 = 1.0F;
    const std::array<float, 4> p{-dx, dx, -dy, dy};
    const std::array<float, 4> q{x0 - rx0, rx1 - x0, y0 - ry0, ry1 - y0};
    for (std::size_t i = 0; i < p.size(); ++i) {
        if (p.at(i) == 0.0F) {
            if (q.at(i) < 0.0F) {
                return false; // parallel and outside
            }
            continue;
        }
        const float t = q.at(i) / p.at(i);
        if (p.at(i) < 0.0F) {
            t0 = std::max(t0, t);
        } else {
            t1 = std::min(t1, t);
        }
    }
    if (t0 > t1) {
        return false;
    }
    const float sx = x0;
    const float sy = y0;
    x0 = sx + (dx * t0);
    y0 = sy + (dy * t0);
    x1 = sx + (dx * t1);
    y1 = sy + (dy * t1);
    return true;
}

// A code-drawn heart: two lobes and a point, `size` tall.
void drawHeart(Canvas& canvas, float cx, float cy, float size, Color color) {
    const float lobe = size * 0.28F;
    canvas.fillCircle(cx - (size * 0.25F), cy - (size * 0.18F), lobe, color);
    canvas.fillCircle(cx + (size * 0.25F), cy - (size * 0.18F), lobe, color);
    const std::array<Canvas::Vertex, 3> tip{
        Canvas::Vertex{.x = cx - (size * 0.5F), .y = cy - (size * 0.08F), .color = color},
        Canvas::Vertex{.x = cx + (size * 0.5F), .y = cy - (size * 0.08F), .color = color},
        Canvas::Vertex{.x = cx, .y = cy + (size * 0.5F), .color = color},
    };
    canvas.fillConvexPolygon(tip);
}

[[nodiscard]] Color mixColor(Color a, Color b, float t) {
    const auto ch = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(ease::lerp(x, y, ease::clampUnit(t))));
    };
    return rgb(ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a));
}

} // namespace

int arrowsSavedLevel(Difficulty difficulty) {
    return savedLevelField(settings(), difficulty);
}

ArrowsScene::ArrowsScene(SceneManager& manager, Difficulty difficulty, int level)
    : manager_(manager), difficulty_(difficulty), level_(std::max(1, level)),
      board_(arrowsBoardFor(difficulty, level_)),
      backButton_(IconButton::Icon::Back, kBackCx, kBackCy, kBackRadius),
      resetButton_(IconButton::Icon::Glyph, kResetCx, kResetCy, kResetRadius),
      overlay_(color(difficulty_), colors::white, kButtonRowY) {
    backButton_.setOnTap([this] { manager_.pop(); });
    resetButton_.setGlyph(kReset, 54.0F);
    resetButton_.setOnTap([this] { restart(); });
    overlay_.setOnHome([this] { manager_.popToRoot(); });
    heartLost_.fill(-1.0F);
    layoutBoard();
}

void ArrowsScene::layoutBoard() {
    const grid::Fit fit = grid::fitCentered(kAreaX, kAreaTop, kAreaW, kAreaH, board_.width(),
                                            board_.height(), kMaxCellPx);
    cellPx_ = fit.cellPx;
    originX_ = fit.originX;
    originY_ = fit.originY;
}

ArrowsScene::Pt ArrowsScene::cellCenter(ArrowCell cell) const {
    return {.x = originX_ + ((static_cast<float>(cell.x) + 0.5F) * cellPx_),
            .y = originY_ + ((static_cast<float>(cell.y) + 0.5F) * cellPx_)};
}

float ArrowsScene::drawnLength(const Arrow& arrow) const {
    return (kTailExt + static_cast<float>(arrow.cells.size() - 1) + kTipExt) * cellPx_;
}

std::vector<ArrowsScene::Pt> ArrowsScene::trajectory(const Arrow& arrow) const {
    std::vector<Pt> path;
    path.reserve(arrow.cells.size() + 2);
    // Behind the tail: continue the first segment (or the heading, for a
    // one-cell arrow) a little so the shaft fills the tail cell.
    const ArrowCell tail = arrow.cells.front();
    const Pt tailCenter = cellCenter(tail);
    auto bx = static_cast<float>(dirDx(arrow.dir));
    auto by = static_cast<float>(dirDy(arrow.dir));
    if (arrow.cells.size() >= 2) {
        bx = static_cast<float>(arrow.cells.at(1).x - tail.x);
        by = static_cast<float>(arrow.cells.at(1).y - tail.y);
    }
    path.push_back({.x = tailCenter.x - (bx * kTailExt * cellPx_),
                    .y = tailCenter.y - (by * kTailExt * cellPx_)});
    for (const ArrowCell& c : arrow.cells) {
        path.push_back(cellCenter(c));
    }
    // Ahead of the head: straight out past the edge, far enough that the whole
    // drawn length fits beyond the board.
    const ArrowCell head = arrow.cells.back();
    int steps = 0;
    for (int x = head.x + dirDx(arrow.dir), y = head.y + dirDy(arrow.dir); board_.inBounds(x, y);
         x += dirDx(arrow.dir), y += dirDy(arrow.dir)) {
        ++steps;
    }
    const float exit = ((static_cast<float>(steps) + 1.5F) * cellPx_) + drawnLength(arrow);
    const Pt headCenter = cellCenter(head);
    path.push_back({.x = headCenter.x + (static_cast<float>(dirDx(arrow.dir)) * exit),
                    .y = headCenter.y + (static_cast<float>(dirDy(arrow.dir)) * exit)});
    return path;
}

ArrowsScene::Flight ArrowsScene::makeFlight(const Arrow& arrow) const {
    Flight flight;
    flight.path = trajectory(arrow);
    flight.length = drawnLength(arrow);
    // The tail has left once it passes the edge: its own run to the head, the
    // head's run to the edge, and a little margin.
    const std::size_t n = flight.path.size();
    const float toHead = (kTailExt + static_cast<float>(arrow.cells.size() - 1)) * cellPx_;
    const float headToExit = distance(flight.path.at(n - 2).x, flight.path.at(n - 2).y,
                                      flight.path.at(n - 1).x, flight.path.at(n - 1).y);
    flight.end = toHead + headToExit - flight.length - (0.5F * cellPx_);
    return flight;
}

int ArrowsScene::arrowNear(float px, float py) const {
    const grid::Fit fit{.cellPx = cellPx_, .originX = originX_, .originY = originY_};
    int cx = 0;
    int cy = 0;
    if (!grid::cellAt(fit, px, py, cx, cy)) {
        return -1;
    }
    // The tapped cell first; otherwise the nearest covered neighbour within
    // reach, so a finger landing just beside a thin shaft still counts.
    if (const int hit = board_.arrowAt(cx, cy); hit >= 0) {
        return hit;
    }
    int best = -1;
    float bestDist = kTouchReach * cellPx_;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int other = board_.arrowAt(cx + dx, cy + dy);
            if (other < 0) {
                continue;
            }
            const Pt c = cellCenter({.x = cx + dx, .y = cy + dy});
            const float d = distance(px, py, c.x, c.y);
            if (d < bestDist) {
                bestDist = d;
                best = other;
            }
        }
    }
    return best;
}

void ArrowsScene::tapAt(float px, float py) {
    const int index = arrowNear(px, py);
    if (index < 0) {
        return;
    }
    // A shaking arrow is mid-feedback; do not charge a second heart for it.
    if (std::ranges::any_of(shakes_, [index](const Shake& s) { return s.arrow == index; })) {
        return;
    }
    const Arrow arrow = board_.arrows().at(static_cast<std::size_t>(index));
    const int blocker = board_.blockerOf(index);
    switch (board_.tap(index)) {
    case ArrowsBoard::Tap::Cleared:
        flights_.push_back(makeFlight(arrow));
        break;
    case ArrowsBoard::Tap::Blocked:
        shakes_.push_back({.arrow = index, .blocker = blocker, .t = 0.0F});
        heartLost_.at(static_cast<std::size_t>(board_.hearts())) = 0.0F;
        break;
    case ArrowsBoard::Tap::Ignored:
        break;
    }
}

void ArrowsScene::restart() {
    board_.reset();
    flights_.clear();
    shakes_.clear();
    heartLost_.fill(-1.0F);
    phase_ = Phase::Playing;
}

void ArrowsScene::handleInput(const PointerEvent& event) {
    if (backButton_.handleInput(event)) {
        return;
    }
    if (phase_ != Phase::Playing) {
        overlay_.handleInput(event);
        return;
    }
    if (resetButton_.handleInput(event)) {
        return;
    }
    if (event.phase == PointerEvent::Phase::Down) {
        tapAt(event.x, event.y);
    }
}

void ArrowsScene::finishIfOver() {
    if (phase_ != Phase::Playing || !flights_.empty() || !shakes_.empty()) {
        return;
    }
    if (board_.isWon()) {
        phase_ = Phase::Won;
        if (level_ + 1 > arrowsSavedLevel(difficulty_)) {
            setSavedLevel(difficulty_, level_ + 1);
        }
        overlay_.setActionLabel("NEXT");
        overlay_.setOnAction([this] {
            manager_.replace(std::make_unique<ArrowsScene>(manager_, difficulty_, level_ + 1));
        });
    } else if (board_.isLost()) {
        phase_ = Phase::Lost;
        overlay_.setActionLabel("RETRY");
        overlay_.setOnAction([this] { restart(); });
    }
}

void ArrowsScene::update(float dtSeconds) {
    for (Flight& f : flights_) {
        f.s += kFlyCellsPerSecond * cellPx_ * dtSeconds;
    }
    std::erase_if(flights_, [](const Flight& f) { return f.s >= f.end; });
    for (Shake& s : shakes_) {
        s.t += dtSeconds;
    }
    std::erase_if(shakes_, [](const Shake& s) { return s.t >= kFlashSeconds; });
    for (float& t : heartLost_) {
        if (t >= 0.0F && t < kHeartPopSeconds) {
            t = std::min(kHeartPopSeconds, t + dtSeconds);
        }
    }
    finishIfOver();
}

bool ArrowsScene::isAnimating() const {
    const bool popping =
        std::ranges::any_of(heartLost_, [](float t) { return t >= 0.0F && t < kHeartPopSeconds; });
    return !flights_.empty() || !shakes_.empty() || popping;
}

float ArrowsScene::shakeOffset(int arrow) const {
    for (const Shake& s : shakes_) {
        if (s.arrow == arrow && s.t < kBumpSeconds) {
            return kBumpCells * cellPx_ *
                   std::sin(std::numbers::pi_v<float> * (s.t / kBumpSeconds));
        }
    }
    return 0.0F;
}

Color ArrowsScene::arrowColor(int arrow) const {
    for (const Shake& s : shakes_) {
        if (s.arrow == arrow || s.blocker == arrow) {
            // Solid red, fading back to ink over the last part of the flash.
            const float fade = (s.t - (kFlashSeconds * 0.6F)) / (kFlashSeconds * 0.4F);
            return mixColor(colors::hardRed, theme().arInk, fade);
        }
    }
    return theme().arInk;
}

// ---- Drawing ----------------------------------------------------------------

void ArrowsScene::drawTopBar(Canvas& canvas) const {
    backButton_.render(canvas);
    resetButton_.render(canvas);
    canvas.textCentered(label(difficulty_), layout::kWidthF / 2.0F, kDiffLabelCy, 30.0F,
                        color(difficulty_));
    canvas.textCentered("Level " + std::to_string(level_), layout::kWidthF / 2.0F, kLevelLabelCy,
                        56.0F, theme().titleText);
    drawHearts(canvas);
}

void ArrowsScene::drawHearts(Canvas& canvas) const {
    const float startX = (layout::kWidthF / 2.0F) -
                         (kHeartPitch * (static_cast<float>(ArrowsBoard::kHearts) - 1.0F) / 2.0F);
    for (int i = 0; i < ArrowsBoard::kHearts; ++i) {
        const float cx = startX + (kHeartPitch * static_cast<float>(i));
        const float lost = heartLost_.at(static_cast<std::size_t>(i));
        float size = kHeartSize;
        Color color = colors::youRed;
        if (lost >= 0.0F) {
            // Pop: swell, then settle small and pale.
            const float t = ease::clampUnit(lost / kHeartPopSeconds);
            size = kHeartSize * (1.0F + (0.45F * std::sin(std::numbers::pi_v<float> * t)));
            color = mixColor(colors::youRed, theme().arHeartLost, t);
        }
        drawHeart(canvas, cx, kHeartsCy, size, color);
    }
}

void ArrowsScene::drawDots(Canvas& canvas) const {
    const float r = std::max(2.0F, cellPx_ * kDotRadius);
    for (int y = 0; y < board_.height(); ++y) {
        for (int x = 0; x < board_.width(); ++x) {
            const Pt c = cellCenter({.x = x, .y = y});
            canvas.fillCircle(c.x, c.y, r, theme().arDot);
        }
    }
}

void ArrowsScene::drawArrowWindow(Canvas& canvas, const std::vector<Pt>& path, float s0, float s1,
                                  Color color) const {
    // 1. Cut the window [s0, s1] out of the trajectory, keeping only corners.
    std::vector<Pt> pts;
    float acc = 0.0F;
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        const Pt a = path.at(i);
        const Pt b = path.at(i + 1);
        const float len = distance(a.x, a.y, b.x, b.y);
        const float segStart = acc;
        const float segEnd = acc + len;
        acc = segEnd;
        if (segEnd < s0 || len <= 0.0F) {
            continue;
        }
        if (segStart >= s1) {
            break;
        }
        if (pts.empty()) {
            const float t = ease::clampUnit((s0 - segStart) / len);
            pts.push_back({.x = ease::lerp(a.x, b.x, t), .y = ease::lerp(a.y, b.y, t)});
        }
        if (segEnd <= s1) {
            pts.push_back(b);
        } else {
            const float t = ease::clampUnit((s1 - segStart) / len);
            pts.push_back({.x = ease::lerp(a.x, b.x, t), .y = ease::lerp(a.y, b.y, t)});
            break;
        }
    }
    if (pts.size() < 2) {
        return;
    }
    // Drop points that sit on a straight run so joints only go at corners.
    std::vector<Pt> corners{pts.front()};
    for (std::size_t i = 1; i + 1 < pts.size(); ++i) {
        const Pt& a = corners.back();
        const Pt& b = pts.at(i);
        const Pt& c = pts.at(i + 1);
        const float cross = ((b.x - a.x) * (c.y - b.y)) - ((b.y - a.y) * (c.x - b.x));
        if (std::fabs(cross) > 0.01F) {
            corners.push_back(b);
        }
    }
    corners.push_back(pts.back());

    // 2. Shaft: round tail cap, segments, discs at the corners; everything
    //    clipped to the board rectangle.
    const float stroke = std::clamp(cellPx_ * kStroke, kMinStrokePx, kMaxStrokePx);
    const float margin = cellPx_ * kClipMargin;
    const float rx0 = originX_ - margin;
    const float ry0 = originY_ - margin;
    const float rx1 = originX_ + (static_cast<float>(board_.width()) * cellPx_) + margin;
    const float ry1 = originY_ + (static_cast<float>(board_.height()) * cellPx_) + margin;
    const auto inside = [&](const Pt& p) {
        return p.x >= rx0 && p.x <= rx1 && p.y >= ry0 && p.y <= ry1;
    };
    const auto strokeLine = [&](Pt a, Pt b) {
        if (clipSegment(rx0, ry0, rx1, ry1, a.x, a.y, b.x, b.y)) {
            canvas.line(a.x, a.y, b.x, b.y, stroke, color);
        }
    };
    const auto joint = [&](const Pt& p) {
        if (inside(p)) {
            canvas.fillCircle(p.x, p.y, stroke / 2.0F, color);
        }
    };
    joint(corners.front());
    for (std::size_t i = 0; i + 1 < corners.size(); ++i) {
        strokeLine(corners.at(i), corners.at(i + 1));
        joint(corners.at(i + 1));
    }

    // 3. Chevron head at the tip, pointing along the last segment.
    const Pt tip = corners.back();
    const Pt prev = corners.at(corners.size() - 2);
    const float len = distance(prev.x, prev.y, tip.x, tip.y);
    if (len <= 0.0F) {
        return;
    }
    const float ux = (tip.x - prev.x) / len;
    const float uy = (tip.y - prev.y) / len;
    const float wing = cellPx_ * kWing;
    const Pt left{.x = tip.x - (ux * wing) - (uy * wing), .y = tip.y - (uy * wing) + (ux * wing)};
    const Pt right{.x = tip.x - (ux * wing) + (uy * wing), .y = tip.y - (uy * wing) - (ux * wing)};
    strokeLine(left, tip);
    strokeLine(right, tip);
    joint(left);
    joint(right);
}

void ArrowsScene::drawArrows(Canvas& canvas) const {
    for (int i = 0; i < board_.arrowCount(); ++i) {
        if (board_.isRemoved(i)) {
            continue;
        }
        const Arrow& arrow = board_.arrows().at(static_cast<std::size_t>(i));
        const float s0 = shakeOffset(i);
        drawArrowWindow(canvas, trajectory(arrow), s0, s0 + drawnLength(arrow), arrowColor(i));
    }
}

void ArrowsScene::drawFlights(Canvas& canvas) const {
    for (const Flight& f : flights_) {
        drawArrowWindow(canvas, f.path, f.s, f.s + f.length, theme().arInk);
    }
}

void ArrowsScene::drawOverlay(Canvas& canvas) const {
    if (phase_ == Phase::Won) {
        overlay_.render(canvas, "CLEARED!", 600.0F, 96.0F);
    } else {
        overlay_.render(canvas, "OUT OF HEARTS", 600.0F, 72.0F);
    }
}

void ArrowsScene::render(Canvas& canvas) {
    canvas.clear(theme().arField);
    drawDots(canvas);
    drawArrows(canvas);
    drawFlights(canvas);
    drawTopBar(canvas);
    if (phase_ != Phase::Playing) {
        drawOverlay(canvas);
    }
}

} // namespace og
