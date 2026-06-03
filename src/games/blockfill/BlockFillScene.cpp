#include "games/blockfill/BlockFillScene.hpp"

#include "core/Canvas.hpp"
#include "core/Color.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"
#include "games/blockfill/BlockFillLevels.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
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

// ---- Top-bar title ---------------------------------------------------------
constexpr float kDiffLabelCy = 70.0F;
constexpr float kLevelLabelCy = 132.0F;

// ---- Board play area: the grid is fitted/centred into this rect -------------
constexpr float kAreaX = 40.0F;
constexpr float kAreaTop = 232.0F;
constexpr float kAreaBottom = 1240.0F;
constexpr float kAreaW = layout::kWidthF - (2.0F * kAreaX);
constexpr float kAreaH = kAreaBottom - kAreaTop;
constexpr float kMaxCellPx = 96.0F;

// ---- Solved overlay buttons — same layout as the other games ---------------
constexpr float kButtonRowY = 820.0F;
constexpr float kHomeSize = 140.0F;
constexpr float kNextW = 360.0F;
constexpr float kButtonGap = 24.0F;
constexpr float kRowWidth = kHomeSize + kButtonGap + kNextW;
constexpr float kRowX = (layout::kWidthF - kRowWidth) / 2.0F;

constexpr const char* kHome = "\xF0\x9F\x8F\xA0";  // 🏠
constexpr const char* kReset = "\xF0\x9F\x94\x84"; // 🔄

// Difficulty -> board pool index (Easy=0, Medium=1, Hard=2, VeryHard=3).
[[nodiscard]] int difficultyTier(Difficulty difficulty) {
    return static_cast<int>(difficulty);
}

// The Settings field holding the current level for a difficulty.
[[nodiscard]] int& savedLevelField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.blockfillLevelEasy;
    case Difficulty::Medium:
        return s.blockfillLevelMedium;
    case Difficulty::Hard:
        return s.blockfillLevelHard;
    case Difficulty::VeryHard:
        return s.blockfillLevelVeryHard;
    }
    return s.blockfillLevelEasy;
}

void setSavedLevel(Difficulty difficulty, int level) {
    savedLevelField(settings(), difficulty) = std::max(1, level);
    saveSettings(settings());
}

// Build the board for (difficulty, 1-based level) from that difficulty's pool of
// original boards. Falls back to a tiny full 3x3 board if the asset is missing.
[[nodiscard]] BlockFillBoard boardFor(Difficulty difficulty, int level) {
    const BlockFillLevel* layout = blockFillTierLevel(difficultyTier(difficulty), level);
    if (layout != nullptr) {
        return {layout->width, layout->height, layout->playable, layout->startX, layout->startY};
    }
    static const std::vector<std::uint8_t> kFallback(9, 1); // 3x3, all playable
    return {3, 3, kFallback, 0, 0};
}

} // namespace

int blockFillSavedLevel(Difficulty difficulty) {
    return savedLevelField(settings(), difficulty);
}

BlockFillScene::BlockFillScene(SceneManager& manager, Difficulty difficulty, int level)
    : manager_(manager), difficulty_(difficulty), level_(std::max(1, level)),
      board_(boardFor(difficulty, level)),
      homeButton_(kHome, kRowX, kButtonRowY, kHomeSize, kHomeSize),
      nextButton_("NEXT", kRowX + kHomeSize + kButtonGap, kButtonRowY, kNextW, kHomeSize) {
    homeButton_.setColors(colors::white, colors::panelBrown);
    homeButton_.setOnTap([this] { manager_.popToRoot(); });
    nextButton_.setColors(color(difficulty_), colors::white);
    // The action button's label/handler are set per result in onSolved().
    layoutBoard();
}

void BlockFillScene::layoutBoard() {
    const auto gw = static_cast<float>(board_.width());
    const auto gh = static_cast<float>(board_.height());
    cellPx_ = std::min({kAreaW / gw, kAreaH / gh, kMaxCellPx});
    originX_ = kAreaX + ((kAreaW - (gw * cellPx_)) / 2.0F);
    originY_ = kAreaTop + ((kAreaH - (gh * cellPx_)) / 2.0F);
}

bool BlockFillScene::cellAt(float px, float py, int& x, int& y) const {
    if (px < originX_ || py < originY_) {
        return false;
    }
    const int c = static_cast<int>((px - originX_) / cellPx_);
    const int r = static_cast<int>((py - originY_) / cellPx_);
    if (!board_.isPlayable(c, r)) {
        return false;
    }
    x = c;
    y = r;
    return true;
}

float BlockFillScene::cellCenterX(int x) const {
    return originX_ + ((static_cast<float>(x) + 0.5F) * cellPx_);
}

float BlockFillScene::cellCenterY(int y) const {
    return originY_ + ((static_cast<float>(y) + 0.5F) * cellPx_);
}

bool BlockFillScene::handleBackButton(const PointerEvent& event) {
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

bool BlockFillScene::handleResetButton(const PointerEvent& event) {
    if (event.phase == PointerEvent::Phase::Move) {
        return false;
    }
    const bool inside = hitTest(event, kResetCx - kResetRadius, kResetCy - kResetRadius,
                                kResetRadius * 2.0F, kResetRadius * 2.0F);
    if (event.phase == PointerEvent::Phase::Down) {
        resetPressed_ = inside;
        return inside;
    }
    const bool wasPressed = resetPressed_;
    resetPressed_ = false;
    if (wasPressed && inside) {
        board_.reset();
        dragging_ = false;
        return true;
    }
    return false;
}

void BlockFillScene::handleDrag(const PointerEvent& event) {
    int x = 0;
    int y = 0;
    switch (event.phase) {
    case PointerEvent::Phase::Down:
        // Grab the rope: press anywhere on it to start dragging from there,
        // cutting it back to the touched cell (tap the start dot to clear it).
        if (cellAt(event.x, event.y, x, y) && board_.pathContains(x, y)) {
            dragging_ = true;
            board_.truncateTo(x, y);
            lastCellX_ = x;
            lastCellY_ = y;
        }
        break;
    case PointerEvent::Phase::Move:
        if (dragging_ && cellAt(event.x, event.y, x, y) && (x != lastCellX_ || y != lastCellY_)) {
            lastCellX_ = x;
            lastCellY_ = y;
            if (board_.pathContains(x, y)) {
                board_.truncateTo(x, y); // dragged back onto the rope -> cut here
            } else {
                board_.dragToward(x, y); // extend toward a fresh cell
            }
            if (board_.isSolved()) {
                onSolved();
            }
        }
        break;
    case PointerEvent::Phase::Up:
        dragging_ = false;
        break;
    }
}

void BlockFillScene::handleInput(const PointerEvent& event) {
    if (handleBackButton(event)) {
        return;
    }
    if (phase_ == Phase::Solved) {
        if (homeButton_.handleInput(event)) {
            return;
        }
        nextButton_.handleInput(event);
        return;
    }
    if (handleResetButton(event)) {
        return;
    }
    handleDrag(event);
}

void BlockFillScene::update(float /*dtSeconds*/) {}

void BlockFillScene::onSolved() {
    if (phase_ == Phase::Solved) {
        return;
    }
    phase_ = Phase::Solved;
    dragging_ = false;

    const int lastLevel = blockFillTierSize(difficultyTier(difficulty_));
    if (level_ < lastLevel && level_ + 1 > blockFillSavedLevel(difficulty_)) {
        setSavedLevel(difficulty_, level_ + 1);
    }
    if (level_ < lastLevel) {
        nextButton_.setLabel("NEXT");
        nextButton_.setOnTap([this] {
            manager_.replace(std::make_unique<BlockFillScene>(manager_, difficulty_, level_ + 1));
        });
    } else {
        nextButton_.setLabel("REPLAY");
        nextButton_.setOnTap([this] {
            board_.reset();
            phase_ = Phase::Playing;
        });
    }
}

void BlockFillScene::drawBackButton(Canvas& canvas) {
    canvas.fillCircle(kBackCx, kBackCy, kBackRadius, theme().backCircle);
    canvas.line(kBackCx + 12.0F, kBackCy - 24.0F, kBackCx - 14.0F, kBackCy, 14.0F, theme().chevron);
    canvas.line(kBackCx - 14.0F, kBackCy, kBackCx + 12.0F, kBackCy + 24.0F, 14.0F, theme().chevron);
}

void BlockFillScene::drawResetButton(Canvas& canvas) {
    canvas.fillCircle(kResetCx, kResetCy, kResetRadius, theme().backCircle);
    canvas.emojiCentered(kReset, kResetCx, kResetCy, 54.0F);
}

void BlockFillScene::drawTopBar(Canvas& canvas) const {
    drawBackButton(canvas);
    drawResetButton(canvas);
    canvas.textCentered(label(difficulty_), layout::kWidthF / 2.0F, kDiffLabelCy, 30.0F,
                        color(difficulty_));
    canvas.textCentered("Level " + std::to_string(level_), layout::kWidthF / 2.0F, kLevelLabelCy,
                        56.0F, theme().primaryText);
}

void BlockFillScene::drawGrid(Canvas& canvas) const {
    const float inset = cellPx_ * 0.06F;
    const float tileW = cellPx_ - (2.0F * inset);
    const float radius = cellPx_ * 0.18F;
    for (int y = 0; y < board_.height(); ++y) {
        for (int x = 0; x < board_.width(); ++x) {
            if (!board_.isPlayable(x, y)) {
                continue; // holes: the dark field shows through
            }
            const float px = originX_ + (static_cast<float>(x) * cellPx_) + inset;
            const float py = originY_ + (static_cast<float>(y) * cellPx_) + inset;
            canvas.fillRoundedRect(px, py, tileW, tileW, radius, theme().bfCell);
        }
    }
}

void BlockFillScene::drawRope(Canvas& canvas) const {
    const std::vector<BlockFillBoard::Cell>& path = board_.path();
    const float inset = cellPx_ * 0.06F;
    const float tileW = cellPx_ - (2.0F * inset);
    const float radius = cellPx_ * 0.18F;
    const float tube = cellPx_ * 0.34F;

    // 1. Light-blue rounded cell bodies under the whole rope.
    for (const BlockFillBoard::Cell& c : path) {
        const float px = originX_ + (static_cast<float>(c.x) * cellPx_) + inset;
        const float py = originY_ + (static_cast<float>(c.y) * cellPx_) + inset;
        canvas.fillRoundedRect(px, py, tileW, tileW, radius, colors::blockFillRope);
    }
    // 2. Darker tube through the cell centres, with a disc at each joint so the
    //    square-capped segments round off at corners.
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        const float x0 = cellCenterX(path.at(i).x);
        const float y0 = cellCenterY(path.at(i).y);
        const float x1 = cellCenterX(path.at(i + 1).x);
        const float y1 = cellCenterY(path.at(i + 1).y);
        canvas.line(x0, y0, x1, y1, tube, colors::blockFillTube);
        canvas.fillCircle(x0, y0, tube / 2.0F, colors::blockFillTube);
    }
    // 3. Round cap at the start anchor and a larger head dot at the rope's end.
    const BlockFillBoard::Cell startCell = path.front();
    canvas.fillCircle(cellCenterX(startCell.x), cellCenterY(startCell.y), tube / 2.0F,
                      colors::blockFillTube);
    const BlockFillBoard::Cell h = board_.head();
    canvas.fillCircle(cellCenterX(h.x), cellCenterY(h.y), cellPx_ * 0.22F, colors::blockFillTube);
}

void BlockFillScene::drawOverlay(Canvas& canvas) const {
    canvas.fillRect(0.0F, 0.0F, layout::kWidthF, layout::kHeightF, colors::overlay);
    canvas.textCentered("SOLVED!", layout::kWidthF / 2.0F, 600.0F, 96.0F, colors::white);
    homeButton_.render(canvas);
    nextButton_.render(canvas);
}

void BlockFillScene::render(Canvas& canvas) {
    canvas.clear(theme().bfField);
    drawTopBar(canvas);
    drawGrid(canvas);
    drawRope(canvas);
    if (phase_ == Phase::Solved) {
        drawOverlay(canvas);
    }
}

} // namespace og
