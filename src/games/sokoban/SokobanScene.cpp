#include "games/sokoban/SokobanScene.hpp"

#include "core/Canvas.hpp"
#include "core/Color.hpp"
#include "core/GridLayout.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"
#include "games/sokoban/SokobanLevels.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace og {
namespace {

constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 92.0F;
constexpr float kTopButtonRadius = 52.0F;
constexpr float kUndoCx = layout::kWidthF - 220.0F;
constexpr float kResetCx = layout::kWidthF - 92.0F;

constexpr float kAreaX = 32.0F;
constexpr float kAreaTop = 278.0F;
constexpr float kAreaBottom = 1028.0F;
constexpr float kAreaW = layout::kWidthF - (2.0F * kAreaX);
constexpr float kAreaH = kAreaBottom - kAreaTop;
constexpr float kMaxCellPx = 74.0F;

constexpr float kDpadCx = layout::kWidthF / 2.0F;
constexpr float kDpadCy = 1238.0F;
constexpr float kDpadGap = 118.0F;
constexpr float kDpadRadius = 54.0F;
constexpr float kButtonRowY = 830.0F;

constexpr Color kWall = rgb(86, 76, 68);
constexpr Color kWallTop = rgb(116, 104, 92);
constexpr Color kGoal = rgb(238, 188, 68);
constexpr Color kBox = rgb(202, 128, 62);
constexpr Color kBoxDone = rgb(86, 178, 118);
constexpr Color kBoxLine = rgb(116, 72, 42);
constexpr Color kPlayer = rgb(74, 170, 224);
constexpr Color kPlayerFace = rgb(244, 246, 250);

constexpr const char* kUndoGlyph = "U";
constexpr const char* kResetGlyph = "\xF0\x9F\x94\x84"; // 🔄

[[nodiscard]] int difficultyTier(Difficulty difficulty) {
    return static_cast<int>(difficulty);
}

[[nodiscard]] int& savedLevelField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.sokobanLevelEasy;
    case Difficulty::Medium:
        return s.sokobanLevelMedium;
    case Difficulty::Hard:
        return s.sokobanLevelHard;
    case Difficulty::VeryHard:
        break;
    }
    return s.sokobanLevelHard;
}

void setSavedLevel(Difficulty difficulty, int level) {
    savedLevelField(settings(), difficulty) = std::max(1, level);
    saveSettings(settings());
}

[[nodiscard]] const std::vector<char>& fallbackCells() {
    static const std::vector<char> cells = {
        '#', '#', '#', '#', '#', '#', '@', '$', '.', '#', '#', '#', '#', '#', '#',
    };
    return cells;
}

[[nodiscard]] SokobanBoard boardFor(Difficulty difficulty, int level) {
    const SokobanLevel* layout = sokobanTierLevel(difficultyTier(difficulty), level);
    if (layout != nullptr) {
        return {layout->width, layout->height, layout->cells};
    }
    const std::vector<char>& fallback = fallbackCells();
    return {5, 3, std::span<const char>(fallback.data(), fallback.size())};
}

[[nodiscard]] SokobanBoard::Direction directionFromDelta(int dx, int dy) {
    if (std::abs(dx) > std::abs(dy)) {
        return dx < 0 ? SokobanBoard::Direction::Left : SokobanBoard::Direction::Right;
    }
    return dy < 0 ? SokobanBoard::Direction::Up : SokobanBoard::Direction::Down;
}

} // namespace

int sokobanSavedLevel(Difficulty difficulty) {
    return savedLevelField(settings(), difficulty);
}

SokobanScene::SokobanScene(SceneManager& manager, Difficulty difficulty, int level)
    : manager_(manager), difficulty_(difficulty), level_(std::max(1, level)),
      board_(boardFor(difficulty, level_)),
      backButton_(IconButton::Icon::Chevron, kBackCx, kBackCy, kTopButtonRadius),
      undoButton_(IconButton::Icon::Glyph, kUndoCx, kBackCy, kTopButtonRadius),
      resetButton_(IconButton::Icon::Glyph, kResetCx, kBackCy, kTopButtonRadius),
      upButton_(IconButton::Icon::Glyph, kDpadCx, kDpadCy - kDpadGap, kDpadRadius),
      downButton_(IconButton::Icon::Glyph, kDpadCx, kDpadCy + kDpadGap, kDpadRadius),
      leftButton_(IconButton::Icon::Glyph, kDpadCx - kDpadGap, kDpadCy, kDpadRadius),
      rightButton_(IconButton::Icon::Glyph, kDpadCx + kDpadGap, kDpadCy, kDpadRadius),
      overlay_(color(difficulty_), colors::white, kButtonRowY) {
    backButton_.setOnTap([this] { manager_.pop(); });
    undoButton_.setGlyph(kUndoGlyph, 44.0F);
    undoButton_.setOnTap([this] { board_.undo(); });
    resetButton_.setGlyph(kResetGlyph, 50.0F);
    resetButton_.setOnTap([this] { board_.reset(); });
    upButton_.setGlyph("^", 58.0F);
    downButton_.setGlyph("v", 58.0F);
    leftButton_.setGlyph("<", 58.0F);
    rightButton_.setGlyph(">", 58.0F);
    upButton_.setOnTap([this] { tryMove(SokobanBoard::Direction::Up); });
    downButton_.setOnTap([this] { tryMove(SokobanBoard::Direction::Down); });
    leftButton_.setOnTap([this] { tryMove(SokobanBoard::Direction::Left); });
    rightButton_.setOnTap([this] { tryMove(SokobanBoard::Direction::Right); });
    overlay_.setOnHome([this] { manager_.popToRoot(); });

    if (const SokobanLevel* layout = sokobanTierLevel(difficultyTier(difficulty_), level_);
        layout != nullptr) {
        sourceSet_ = layout->sourceSet;
        sourceLevel_ = layout->sourceLevel;
    }
    layoutBoard();
}

void SokobanScene::layoutBoard() {
    const grid::Fit fit = grid::fitCentered(kAreaX, kAreaTop, kAreaW, kAreaH, board_.width(),
                                            board_.height(), kMaxCellPx);
    cellPx_ = fit.cellPx;
    originX_ = fit.originX;
    originY_ = fit.originY;
}

bool SokobanScene::cellAt(float px, float py, int& x, int& y) const {
    const grid::Fit fit{.cellPx = cellPx_, .originX = originX_, .originY = originY_};
    return grid::cellAt(fit, px, py, x, y) && x >= 0 && x < board_.width() && y >= 0 &&
           y < board_.height();
}

float SokobanScene::cellX(int x) const {
    return originX_ + (static_cast<float>(x) * cellPx_);
}

float SokobanScene::cellY(int y) const {
    return originY_ + (static_cast<float>(y) * cellPx_);
}

float SokobanScene::cellCenterX(int x) const {
    return cellX(x) + (cellPx_ / 2.0F);
}

float SokobanScene::cellCenterY(int y) const {
    return cellY(y) + (cellPx_ / 2.0F);
}

void SokobanScene::tryMove(SokobanBoard::Direction direction) {
    if (phase_ != Phase::Playing) {
        return;
    }
    if (board_.tryMove(direction) && board_.isSolved()) {
        onSolved();
    }
}

void SokobanScene::moveFromBoardTap(float px, float py) {
    int x = 0;
    int y = 0;
    if (!cellAt(px, py, x, y)) {
        return;
    }
    const SokobanBoard::Cell player = board_.player();
    if (x == player.x && y == player.y) {
        return;
    }
    tryMove(directionFromDelta(x - player.x, y - player.y));
}

void SokobanScene::handleInput(const PointerEvent& event) {
    if (backButton_.handleInput(event)) {
        return;
    }
    if (phase_ == Phase::Solved) {
        overlay_.handleInput(event);
        return;
    }
    if (undoButton_.handleInput(event) || resetButton_.handleInput(event) ||
        upButton_.handleInput(event) || downButton_.handleInput(event) ||
        leftButton_.handleInput(event) || rightButton_.handleInput(event)) {
        return;
    }
    if (event.phase == PointerEvent::Phase::Down) {
        moveFromBoardTap(event.x, event.y);
    }
}

void SokobanScene::update(float /*dtSeconds*/) {}

void SokobanScene::onSolved() {
    if (phase_ == Phase::Solved) {
        return;
    }
    phase_ = Phase::Solved;

    const int lastLevel = std::max(1, sokobanTierSize(difficultyTier(difficulty_)));
    if (level_ < lastLevel && level_ + 1 > sokobanSavedLevel(difficulty_)) {
        setSavedLevel(difficulty_, level_ + 1);
    }
    if (level_ < lastLevel) {
        overlay_.setActionLabel("NEXT");
        overlay_.setOnAction([this] {
            manager_.replace(std::make_unique<SokobanScene>(manager_, difficulty_, level_ + 1));
        });
    } else {
        overlay_.setActionLabel("REPLAY");
        overlay_.setOnAction([this] {
            board_.reset();
            phase_ = Phase::Playing;
        });
    }
}

void SokobanScene::drawTopBar(Canvas& canvas) const {
    backButton_.render(canvas);
    undoButton_.render(canvas);
    resetButton_.render(canvas);
    canvas.textCentered(label(difficulty_), layout::kWidthF / 2.0F, 52.0F, 28.0F,
                        color(difficulty_));
    canvas.textCentered("Level " + std::to_string(level_), layout::kWidthF / 2.0F, 104.0F, 48.0F,
                        theme().primaryText);

    const std::string source =
        std::string(sokobanSetName(sourceSet_)) + " " + std::to_string(sourceLevel_);
    canvas.textCentered(source, layout::kWidthF / 2.0F, 154.0F, 24.0F, colors::textMuted);

    const std::string stats =
        "Moves " + std::to_string(board_.moves()) + "  Pushes " + std::to_string(board_.pushes());
    canvas.textCentered(stats, layout::kWidthF / 2.0F, 204.0F, 26.0F, colors::textMuted);
}

void SokobanScene::drawGoals(Canvas& canvas) const {
    const float outer = std::max(2.0F, cellPx_ * 0.22F);
    const float inner = std::max(1.0F, outer * 0.45F);
    for (int y = 0; y < board_.height(); ++y) {
        for (int x = 0; x < board_.width(); ++x) {
            if (board_.isGoal(x, y)) {
                const float cx = cellCenterX(x);
                const float cy = cellCenterY(y);
                canvas.fillCircle(cx, cy, outer, kGoal);
                canvas.fillCircle(cx, cy, inner, theme().bfField);
            }
        }
    }
}

void SokobanScene::drawBoxes(Canvas& canvas) const {
    const float inset = std::max(1.0F, cellPx_ * 0.10F);
    const float size = std::max(2.0F, cellPx_ - (2.0F * inset));
    const float radius = std::max(1.0F, cellPx_ * 0.14F);
    const float line = std::max(1.0F, cellPx_ * 0.055F);
    for (int y = 0; y < board_.height(); ++y) {
        for (int x = 0; x < board_.width(); ++x) {
            if (!board_.hasBox(x, y)) {
                continue;
            }
            const float px = cellX(x) + inset;
            const float py = cellY(y) + inset;
            const Color fill = board_.isGoal(x, y) ? kBoxDone : kBox;
            canvas.fillRoundedRect(px, py, size, size, radius, fill);
            if (cellPx_ >= 18.0F) {
                canvas.line(px + (size * 0.22F), py + (size * 0.22F), px + (size * 0.78F),
                            py + (size * 0.78F), line, kBoxLine);
                canvas.line(px + (size * 0.78F), py + (size * 0.22F), px + (size * 0.22F),
                            py + (size * 0.78F), line, kBoxLine);
            }
        }
    }
}

void SokobanScene::drawPlayer(Canvas& canvas) const {
    const SokobanBoard::Cell player = board_.player();
    const float cx = cellCenterX(player.x);
    const float cy = cellCenterY(player.y);
    const float body = std::max(3.0F, cellPx_ * 0.32F);
    canvas.fillCircle(cx, cy, body, kPlayer);
    if (cellPx_ >= 16.0F) {
        canvas.fillCircle(cx - (body * 0.28F), cy - (body * 0.18F), body * 0.16F, kPlayerFace);
        canvas.fillCircle(cx + (body * 0.28F), cy - (body * 0.18F), body * 0.16F, kPlayerFace);
    }
}

void SokobanScene::drawBoard(Canvas& canvas) const {
    canvas.fillRoundedRect(kAreaX, kAreaTop, kAreaW, kAreaH, 18.0F, theme().bfField);
    const float gap = std::max(0.5F, cellPx_ * 0.035F);
    const float tile = std::max(1.0F, cellPx_ - (2.0F * gap));
    const float radius = std::max(1.0F, cellPx_ * 0.10F);
    for (int y = 0; y < board_.height(); ++y) {
        for (int x = 0; x < board_.width(); ++x) {
            if (!board_.isWall(x, y)) {
                continue;
            }
            const float px = cellX(x) + gap;
            const float py = cellY(y) + gap;
            canvas.fillRoundedRect(px, py, tile, tile, radius, kWall);
            if (cellPx_ >= 12.0F) {
                canvas.fillRect(px, py, tile, std::max(1.0F, tile * 0.22F), kWallTop);
            }
        }
    }
    drawGoals(canvas);
    drawBoxes(canvas);
    drawPlayer(canvas);
}

void SokobanScene::drawDpad(Canvas& canvas) const {
    upButton_.render(canvas);
    downButton_.render(canvas);
    leftButton_.render(canvas);
    rightButton_.render(canvas);
}

void SokobanScene::drawOverlay(Canvas& canvas) const {
    overlay_.render(canvas, "SOLVED!", 610.0F, 92.0F);
}

void SokobanScene::render(Canvas& canvas) {
    canvas.clear(theme().bfField);
    drawTopBar(canvas);
    drawBoard(canvas);
    drawDpad(canvas);
    if (phase_ == Phase::Solved) {
        drawOverlay(canvas);
    }
}

} // namespace og
