#include "games/twentyfortyeight/TwentyFortyEightScene.hpp"

#include "core/Canvas.hpp"
#include "core/Easing.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <string>

namespace og {
namespace {

using Board = TwentyFortyEightBoard;
using Direction = Board::Direction;

// ---- Top bar ----------------------------------------------------------------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;
constexpr float kUndoCx = layout::kWidthF - 220.0F;
constexpr float kRestartCx = layout::kWidthF - 92.0F;
// Button glyphs are color emoji: IconButton rasterizes plain text in white, which
// vanishes on the light theme's white disc.
constexpr const char* kUndoGlyph = "\xF0\x9F\x94\x99";    // 🔙
constexpr const char* kRestartGlyph = "\xF0\x9F\x94\x84"; // 🔄
constexpr float kTitleCy = 100.0F;
constexpr float kDifficultyCy = 172.0F;

// ---- Score cards ------------------------------------------------------------
constexpr float kCardY = 214.0F;
constexpr float kCardW = 300.0F;
constexpr float kCardH = 104.0F;
constexpr float kCardGap = 24.0F;
constexpr float kCardX = (layout::kWidthF - (kCardW * 2.0F) - kCardGap) / 2.0F;
constexpr float kGoalCy = 356.0F;

// ---- Board ------------------------------------------------------------------
constexpr float kBoardMargin = 36.0F;
constexpr float kBoardY = 460.0F;
constexpr float kBoardPx = layout::kWidthF - (2.0F * kBoardMargin);
constexpr float kGap = 14.0F; // between cells and around the border
constexpr float kBoardRadius = 18.0F;
constexpr float kHintCy = kBoardY + kBoardPx + 64.0F;

// ---- Animation --------------------------------------------------------------
constexpr float kMergePop = 0.18F;       // merged tile's peak scale overshoot
constexpr float kSwipeThreshold = 40.0F; // logical px of drag that counts as a swipe

// ---- Result overlay ---------------------------------------------------------
constexpr float kButtonRowY = 820.0F;

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

// Best score is persisted per difficulty.
[[nodiscard]] int& bestField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.twentyFortyEightBestEasy;
    case Difficulty::Hard:
        return s.twentyFortyEightBestHard;
    case Difficulty::Medium:
    case Difficulty::VeryHard:
        break;
    }
    return s.twentyFortyEightBestMedium;
}

// The classic 2048 tile palette: warm beiges for 2/4, oranges to red through 64,
// golds from 128 up, and a dark "super" tile past 2048. Identity colors — they
// read on both the light and dark board.
[[nodiscard]] Color tileColor(int value) {
    switch (value) {
    case 2:
        return rgb(238, 228, 218);
    case 4:
        return rgb(237, 224, 200);
    case 8:
        return rgb(242, 177, 121);
    case 16:
        return rgb(245, 149, 99);
    case 32:
        return rgb(246, 124, 95);
    case 64:
        return rgb(246, 94, 59);
    case 128:
        return rgb(237, 207, 114);
    case 256:
        return rgb(237, 204, 97);
    case 512:
        return rgb(237, 200, 80);
    case 1024:
        return rgb(237, 197, 63);
    case 2048:
        return rgb(237, 194, 46);
    default:
        break;
    }
    return rgb(60, 58, 50);
}

[[nodiscard]] Color tileTextColor(int value) {
    return value <= 4 ? rgb(119, 110, 101) : rgb(249, 246, 242);
}

// Bigger numbers get a smaller font so four and five digits still fit the tile.
[[nodiscard]] float tileFontScale(int value) {
    if (value < 100) {
        return 0.46F;
    }
    if (value < 1000) {
        return 0.40F;
    }
    if (value < 10000) {
        return 0.32F;
    }
    return 0.27F;
}

[[nodiscard]] Direction directionFromSwipe(float dx, float dy) {
    if (std::abs(dx) > std::abs(dy)) {
        return dx > 0.0F ? Direction::Right : Direction::Left;
    }
    return dy > 0.0F ? Direction::Down : Direction::Up;
}

} // namespace

TwentyFortyEightScene::TwentyFortyEightScene(SceneManager& manager, Difficulty difficulty)
    : manager_(manager), difficulty_(difficulty),
      params_(twentyFortyEightParams(difficultyToIndex(difficulty))),
      board_(params_.size, std::random_device{}()), best_(bestField(settings(), difficulty)),
      backButton_(IconButton::Icon::Chevron, kBackCx, kBackCy, kBackRadius),
      undoButton_(IconButton::Icon::Glyph, kUndoCx, kBackCy, kBackRadius),
      restartButton_(IconButton::Icon::Glyph, kRestartCx, kBackCy, kBackRadius),
      overlay_(color(difficulty), colors::white, kButtonRowY) {
    const auto n = static_cast<float>(board_.size());
    cellPx_ = (kBoardPx - (kGap * (n + 1.0F))) / n;

    backButton_.setOnTap([this] {
        saveBest();
        manager_.pop();
    });
    undoButton_.setGlyph(kUndoGlyph, 52.0F);
    undoButton_.setOnTap([this] { undoMove(); });
    restartButton_.setGlyph(kRestartGlyph, 50.0F);
    restartButton_.setOnTap([this] { restart(); });
    overlay_.setOnHome([this] {
        saveBest();
        manager_.popToRoot();
    });
    overlay_.setOnAction([this] {
        if (phase_ == Phase::Won) {
            phase_ = Phase::Playing; // keep going: the game is endless past the goal
        } else {
            restart();
        }
    });
}

bool TwentyFortyEightScene::isAnimating() const {
    return animT_ < kAnimTotal;
}

// ---- Input ------------------------------------------------------------------

void TwentyFortyEightScene::handleInput(const PointerEvent& event) {
    if (backButton_.handleInput(event)) {
        return;
    }
    if (phase_ != Phase::Playing) {
        overlay_.handleInput(event);
        return;
    }
    if (undoButton_.handleInput(event) || restartButton_.handleInput(event)) {
        return;
    }
    if (pendingPhase_ != Phase::Playing) {
        return; // the result overlay is about to appear; no more moves
    }
    handleSwipe(event);
}

void TwentyFortyEightScene::handleSwipe(const PointerEvent& event) {
    // One move per press: the first drag past the threshold picks the direction.
    switch (event.phase) {
    case PointerEvent::Phase::Down:
        swiping_ = true;
        swipeX_ = event.x;
        swipeY_ = event.y;
        break;
    case PointerEvent::Phase::Move:
    case PointerEvent::Phase::Up: {
        if (!swiping_) {
            break;
        }
        const float dx = event.x - swipeX_;
        const float dy = event.y - swipeY_;
        if ((dx * dx) + (dy * dy) >= kSwipeThreshold * kSwipeThreshold) {
            swiping_ = false;
            applyMove(directionFromSwipe(dx, dy));
        }
        if (event.phase == PointerEvent::Phase::Up) {
            swiping_ = false;
        }
        break;
    }
    }
}

void TwentyFortyEightScene::applyMove(Direction dir) {
    finishAnimation(); // a quick second swipe snaps the previous move into place
    Board::MoveResult result;
    if (!board_.move(dir, &result)) {
        return;
    }
    lastMove_ = std::move(result);
    animT_ = 0.0F;
    if (board_.score() > best_) {
        best_ = board_.score();
        bestDirty_ = true;
    }
    if (!goalReached_ && board_.maxTile() >= params_.goal) {
        goalReached_ = true;
        pendingPhase_ = Phase::Won;
        overlay_.setActionLabel("KEEP GOING");
        saveBest();
    } else if (!board_.canMove()) {
        pendingPhase_ = Phase::GameOver;
        overlay_.setActionLabel("PLAY AGAIN");
        saveBest();
    }
}

void TwentyFortyEightScene::undoMove() {
    if (phase_ != Phase::Playing || pendingPhase_ != Phase::Playing) {
        return;
    }
    finishAnimation();
    if (board_.undo()) {
        lastMove_ = Board::MoveResult{};
    }
}

void TwentyFortyEightScene::restart() {
    finishAnimation();
    board_.reset();
    lastMove_ = Board::MoveResult{};
    phase_ = Phase::Playing;
    pendingPhase_ = Phase::Playing;
    goalReached_ = false;
    swiping_ = false;
}

void TwentyFortyEightScene::finishAnimation() {
    animT_ = kAnimTotal;
}

void TwentyFortyEightScene::saveBest() {
    if (!bestDirty_) {
        return;
    }
    Settings& s = settings();
    bestField(s, difficulty_) = best_;
    saveSettings(s);
    bestDirty_ = false;
}

// ---- Update -----------------------------------------------------------------

void TwentyFortyEightScene::update(float dtSeconds) {
    if (animT_ < kAnimTotal) {
        animT_ = std::min(animT_ + dtSeconds, kAnimTotal);
    }
    if (animT_ >= kAnimTotal && pendingPhase_ != Phase::Playing) {
        phase_ = pendingPhase_; // show the result only once the last move has settled
        pendingPhase_ = Phase::Playing;
    }
}

// ---- Rendering --------------------------------------------------------------

float TwentyFortyEightScene::cellX(int x) const {
    return kBoardMargin + kGap + (static_cast<float>(x) * (cellPx_ + kGap));
}

float TwentyFortyEightScene::cellY(int y) const {
    return kBoardY + kGap + (static_cast<float>(y) * (cellPx_ + kGap));
}

void TwentyFortyEightScene::drawTopBar(Canvas& canvas) const {
    backButton_.render(canvas);
    undoButton_.render(canvas);
    restartButton_.render(canvas);
    canvas.textCentered("2048", layout::kWidthF / 2.0F, kTitleCy, 60.0F, theme().titleText);
    canvas.textCentered(label(difficulty_), layout::kWidthF / 2.0F, kDifficultyCy, 26.0F,
                        color(difficulty_));
}

void TwentyFortyEightScene::drawScores(Canvas& canvas) const {
    const Color labelInk = rgb(238, 228, 218);
    const auto card = [&](float x, const char* title, int value) {
        canvas.fillRoundedRect(x, kCardY, kCardW, kCardH, 16.0F, theme().tfeBoard);
        const float cx = x + (kCardW / 2.0F);
        canvas.textCentered(title, cx, kCardY + 28.0F, 22.0F, labelInk);
        canvas.textCentered(std::to_string(value), cx, kCardY + 68.0F, 44.0F, colors::white);
    };
    card(kCardX, "SCORE", board_.score());
    card(kCardX + kCardW + kCardGap, "BEST", best_);
    canvas.textCentered("GOAL  " + std::to_string(params_.goal), layout::kWidthF / 2.0F, kGoalCy,
                        24.0F, theme().mutedText);
    canvas.textCentered("Swipe to slide the tiles", layout::kWidthF / 2.0F, kHintCy, 24.0F,
                        theme().mutedText);
}

void TwentyFortyEightScene::drawTile(Canvas& canvas, float cx, float cy, float size, int value) {
    const float half = size / 2.0F;
    canvas.fillRoundedRect(cx - half, cy - half, size, size, std::max(4.0F, size * 0.08F),
                           tileColor(value));
    canvas.textCentered(std::to_string(value), cx, cy, size * tileFontScale(value),
                        tileTextColor(value));
}

void TwentyFortyEightScene::drawSlidingTiles(Canvas& canvas) const {
    // The pre-move tiles glide to their destinations; the spawned tile and the
    // merged sums only appear once they land (drawSettledTiles).
    const float t = ease::easeOutCubic(animT_ / kSlideSeconds);
    const float half = cellPx_ / 2.0F;
    for (const Board::TileMove& m : lastMove_.tiles) {
        const float x = ease::lerp(cellX(m.fromX), cellX(m.toX), t);
        const float y = ease::lerp(cellY(m.fromY), cellY(m.toY), t);
        drawTile(canvas, x + half, y + half, cellPx_, m.value);
    }
}

float TwentyFortyEightScene::settledScale(int x, int y, float u) const {
    if (animT_ >= kAnimTotal) {
        return 1.0F;
    }
    if (x == lastMove_.spawnX && y == lastMove_.spawnY) {
        return std::max(0.0F, ease::easeOutBack(u)); // grows in from nothing
    }
    for (const Board::TileMove& m : lastMove_.tiles) {
        if (m.merged && m.toX == x && m.toY == y) {
            return 1.0F + (kMergePop * std::sin(u * std::numbers::pi_v<float>)); // bulge
        }
    }
    return 1.0F;
}

void TwentyFortyEightScene::drawSettledTiles(Canvas& canvas) const {
    const float u = std::clamp((animT_ - kSlideSeconds) / kPopSeconds, 0.0F, 1.0F);
    const float half = cellPx_ / 2.0F;
    for (int y = 0; y < board_.size(); ++y) {
        for (int x = 0; x < board_.size(); ++x) {
            const int value = board_.at(x, y);
            if (value == 0) {
                continue;
            }
            const float scale = settledScale(x, y, u);
            if (scale <= 0.02F) {
                continue;
            }
            drawTile(canvas, cellX(x) + half, cellY(y) + half, cellPx_ * scale, value);
        }
    }
}

void TwentyFortyEightScene::drawBoard(Canvas& canvas) const {
    canvas.fillRoundedRect(kBoardMargin, kBoardY, kBoardPx, kBoardPx, kBoardRadius,
                           theme().tfeBoard);
    const float radius = std::max(4.0F, cellPx_ * 0.08F);
    for (int y = 0; y < board_.size(); ++y) {
        for (int x = 0; x < board_.size(); ++x) {
            canvas.fillRoundedRect(cellX(x), cellY(y), cellPx_, cellPx_, radius, theme().tfeCell);
        }
    }
    if (animT_ < kSlideSeconds) {
        drawSlidingTiles(canvas);
    } else {
        drawSettledTiles(canvas);
    }
}

void TwentyFortyEightScene::drawOverlay(Canvas& canvas) const {
    overlay_.render(canvas, phase_ == Phase::Won ? "YOU WIN!" : "GAME OVER", 520.0F, 88.0F);
    canvas.textCentered("SCORE  " + std::to_string(board_.score()), layout::kWidthF / 2.0F, 632.0F,
                        40.0F, colors::white);
    canvas.textCentered("BEST  " + std::to_string(best_), layout::kWidthF / 2.0F, 692.0F, 32.0F,
                        colors::menuYellow);
}

void TwentyFortyEightScene::render(Canvas& canvas) {
    canvas.clear(theme().tfeBg);
    drawTopBar(canvas);
    drawScores(canvas);
    drawBoard(canvas);
    if (phase_ != Phase::Playing) {
        drawOverlay(canvas);
    }
}

} // namespace og
