#include "games/minesweeper/MineSweeperScene.hpp"

#include "core/Canvas.hpp"
#include "core/Color.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <string>

namespace og {
namespace {

// ---- Circular chrome buttons (top corners) ---------------------------------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;
constexpr float kResetCx = layout::kWidthF - 92.0F;
constexpr float kResetCy = 100.0F;
constexpr float kResetRadius = 56.0F;

// ---- Streak panels (CURRENT STREAK / ALL TIME) -----------------------------
constexpr float kPanelW = 200.0F;
constexpr float kPanelH = 96.0F;
constexpr float kPanelGap = 20.0F;
constexpr float kPanelY = 54.0F;
constexpr float kPanelsX = (layout::kWidthF - ((2.0F * kPanelW) + kPanelGap)) / 2.0F;
constexpr float kCurrentPanelX = kPanelsX;
constexpr float kAllTimePanelX = kPanelsX + kPanelW + kPanelGap;

// ---- Board play area: the grid is fitted/centred into this rect -------------
constexpr float kAreaX = 30.0F;
constexpr float kAreaTop = 244.0F;
constexpr float kAreaBottom = 1176.0F;
constexpr float kAreaW = layout::kWidthF - (2.0F * kAreaX);
constexpr float kAreaH = kAreaBottom - kAreaTop;
constexpr float kMaxCellPx = 88.0F;

// ---- Bottom bar: mines-left pill + dig/flag toggle --------------------------
constexpr float kBarCy = 1300.0F;
constexpr float kPillX = 44.0F;
constexpr float kPillW = 176.0F;
constexpr float kPillH = 84.0F;
constexpr float kToggleSquare = 104.0F;
constexpr float kTogglePad = 12.0F;
constexpr float kToggleGap = 8.0F;
constexpr float kToggleW = (2.0F * kToggleSquare) + kToggleGap + (2.0F * kTogglePad);
constexpr float kToggleX = (layout::kWidthF - kToggleW) / 2.0F;
constexpr float kToggleH = kToggleSquare + (2.0F * kTogglePad);
constexpr float kDigX = kToggleX + kTogglePad;
constexpr float kFlagX = kDigX + kToggleSquare + kToggleGap;
constexpr float kSquareY = kBarCy - (kToggleSquare / 2.0F);

// ---- Game-over overlay buttons — same layout as the other games ------------
constexpr float kButtonRowY = 820.0F;
constexpr float kHomeSize = 140.0F;
constexpr float kPlayAgainW = 360.0F;
constexpr float kButtonGap = 24.0F;
constexpr float kRowWidth = kHomeSize + kButtonGap + kPlayAgainW;
constexpr float kRowX = (layout::kWidthF - kRowWidth) / 2.0F;

// Emoji glyphs (UTF-8 bytes, matching the project's escaped-literal convention).
constexpr const char* kHome = "\xF0\x9F\x8F\xA0";           // 🏠
constexpr const char* kFlag = "\xF0\x9F\x9A\xA9";           // 🚩
constexpr const char* kBomb = "\xF0\x9F\x92\xA3";           // 💣
constexpr const char* kShovel = "\xE2\x9B\x8F\xEF\xB8\x8F"; // ⛏️ (dig)
constexpr const char* kReset = "\xF0\x9F\x94\x84";          // 🔄
constexpr const char* kCrown = "\xF0\x9F\x91\x91";          // 👑

struct DiffConfig {
    int width;  // columns
    int height; // rows
    int mines;
};

// The original's Easy / Medium / Hard board sizes (80_MINESWEEPER.prefab).
[[nodiscard]] DiffConfig configFor(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return {.width = 8, .height = 8, .mines = 8};
    case Difficulty::Medium:
        return {.width = 9, .height = 10, .mines = 15};
    case Difficulty::Hard:
        return {.width = 9, .height = 14, .mines = 27};
    }
    return {.width = 9, .height = 10, .mines = 15};
}

[[nodiscard]] MineSweeperBoard boardFor(Difficulty difficulty) {
    const DiffConfig cfg = configFor(difficulty);
    return {cfg.width, cfg.height, cfg.mines, std::random_device{}()};
}

// The Settings fields holding a difficulty's current and best win streak.
[[nodiscard]] int& currentStreakField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.minesweeperStreakEasy;
    case Difficulty::Medium:
        return s.minesweeperStreakMedium;
    case Difficulty::Hard:
        return s.minesweeperStreakHard;
    }
    return s.minesweeperStreakMedium;
}

[[nodiscard]] int& bestStreakField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.minesweeperBestEasy;
    case Difficulty::Medium:
        return s.minesweeperBestMedium;
    case Difficulty::Hard:
        return s.minesweeperBestHard;
    }
    return s.minesweeperBestMedium;
}

} // namespace

MineSweeperScene::MineSweeperScene(SceneManager& manager, Difficulty difficulty)
    : manager_(manager), difficulty_(difficulty), board_(boardFor(difficulty)),
      currentStreak_(currentStreakField(settings(), difficulty)),
      bestStreak_(bestStreakField(settings(), difficulty)),
      homeButton_(kHome, kRowX, kButtonRowY, kHomeSize, kHomeSize),
      playAgainButton_("PLAY AGAIN", kRowX + kHomeSize + kButtonGap, kButtonRowY, kPlayAgainW,
                       kHomeSize) {
    homeButton_.setColors(colors::white, colors::panelBrown);
    homeButton_.setOnTap([this] { manager_.popToRoot(); });
    playAgainButton_.setColors(color(difficulty_), colors::white);
    playAgainButton_.setOnTap([this] { beginRound(); });
    layoutBoard();
}

void MineSweeperScene::layoutBoard() {
    const auto gw = static_cast<float>(board_.width());
    const auto gh = static_cast<float>(board_.height());
    cellPx_ = std::min({kAreaW / gw, kAreaH / gh, kMaxCellPx});
    originX_ = kAreaX + ((kAreaW - (gw * cellPx_)) / 2.0F);
    originY_ = kAreaTop + ((kAreaH - (gh * cellPx_)) / 2.0F);
}

bool MineSweeperScene::cellAt(float px, float py, int& row, int& col) const {
    if (px < originX_ || py < originY_) {
        return false;
    }
    const int c = static_cast<int>((px - originX_) / cellPx_);
    const int r = static_cast<int>((py - originY_) / cellPx_);
    if (r < 0 || r >= board_.height() || c < 0 || c >= board_.width()) {
        return false;
    }
    row = r;
    col = c;
    return true;
}

bool MineSweeperScene::handleBackButton(const PointerEvent& event) {
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

bool MineSweeperScene::handleResetButton(const PointerEvent& event) {
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
        beginRound();
        return true;
    }
    return false;
}

bool MineSweeperScene::handleModeToggle(const PointerEvent& event) {
    if (event.phase != PointerEvent::Phase::Down) {
        return false;
    }
    if (hitTest(event, kDigX, kSquareY, kToggleSquare, kToggleSquare)) {
        flagging_ = false;
        return true;
    }
    if (hitTest(event, kFlagX, kSquareY, kToggleSquare, kToggleSquare)) {
        flagging_ = true;
        return true;
    }
    return false;
}

void MineSweeperScene::handleBoardTap(const PointerEvent& event) {
    if (event.phase != PointerEvent::Phase::Down) {
        return;
    }
    int row = 0;
    int col = 0;
    if (!cellAt(event.x, event.y, row, col)) {
        return;
    }
    if (flagging_) {
        board_.toggleFlag(row, col);
    } else if (board_.at(row, col).revealed) {
        board_.chord(row, col); // tap a revealed number to clear its neighbours
    } else {
        board_.reveal(row, col);
    }
    if (board_.state() != MineSweeperBoard::State::Playing) {
        enterGameOver();
    }
}

void MineSweeperScene::handleInput(const PointerEvent& event) {
    if (handleBackButton(event)) {
        return;
    }
    if (phase_ == Phase::GameOver) {
        if (homeButton_.handleInput(event)) {
            return;
        }
        playAgainButton_.handleInput(event);
        return;
    }
    if (handleResetButton(event)) {
        return;
    }
    if (handleModeToggle(event)) {
        return;
    }
    handleBoardTap(event);
}

void MineSweeperScene::update(float /*dtSeconds*/) {}

void MineSweeperScene::beginRound() {
    board_ = boardFor(difficulty_);
    phase_ = Phase::Playing;
    flagging_ = false;
    recorded_ = false;
    layoutBoard();
}

void MineSweeperScene::enterGameOver() {
    if (recorded_) {
        return;
    }
    phase_ = Phase::GameOver;
    if (board_.state() == MineSweeperBoard::State::Won) {
        ++currentStreak_;
        bestStreak_ = std::max(bestStreak_, currentStreak_);
        playAgainButton_.setLabel("PLAY AGAIN");
    } else {
        currentStreak_ = 0;
        playAgainButton_.setLabel("RETRY");
    }
    Settings& s = settings();
    currentStreakField(s, difficulty_) = currentStreak_;
    bestStreakField(s, difficulty_) = bestStreak_;
    saveSettings(s);
    recorded_ = true;
}

const char* MineSweeperScene::resultText() const {
    return board_.state() == MineSweeperBoard::State::Won ? "YOU WIN!" : "BOOM!";
}

void MineSweeperScene::drawBackButton(Canvas& canvas) {
    canvas.fillCircle(kBackCx, kBackCy, kBackRadius, theme().backCircle);
    canvas.line(kBackCx + 12.0F, kBackCy - 24.0F, kBackCx - 14.0F, kBackCy, 14.0F, theme().chevron);
    canvas.line(kBackCx - 14.0F, kBackCy, kBackCx + 12.0F, kBackCy + 24.0F, 14.0F, theme().chevron);
}

void MineSweeperScene::drawResetButton(Canvas& canvas) {
    canvas.fillCircle(kResetCx, kResetCy, kResetRadius, theme().backCircle);
    canvas.emojiCentered(kReset, kResetCx, kResetCy, 54.0F);
}

void MineSweeperScene::drawTopBar(Canvas& canvas) const {
    drawBackButton(canvas);
    drawResetButton(canvas);

    constexpr Color kPanelLabel = rgb(176, 182, 196);
    // CURRENT STREAK.
    canvas.fillRoundedRect(kCurrentPanelX, kPanelY, kPanelW, kPanelH, 20.0F, theme().msPanel);
    canvas.textCentered("CURRENT STREAK", kCurrentPanelX + (kPanelW / 2.0F), kPanelY + 28.0F, 22.0F,
                        kPanelLabel);
    canvas.textCentered(std::to_string(currentStreak_), kCurrentPanelX + (kPanelW / 2.0F),
                        kPanelY + 66.0F, 40.0F, colors::white);
    // ALL TIME (with a small crown perched on its top edge).
    canvas.fillRoundedRect(kAllTimePanelX, kPanelY, kPanelW, kPanelH, 20.0F, theme().msPanel);
    canvas.emojiCentered(kCrown, kAllTimePanelX + (kPanelW / 2.0F), kPanelY, 38.0F);
    canvas.textCentered("ALL TIME", kAllTimePanelX + (kPanelW / 2.0F), kPanelY + 28.0F, 22.0F,
                        kPanelLabel);
    canvas.textCentered(std::to_string(bestStreak_), kAllTimePanelX + (kPanelW / 2.0F),
                        kPanelY + 66.0F, 40.0F, colors::botCyan);
}

void MineSweeperScene::drawBoard(Canvas& canvas) const {
    const float inset = cellPx_ * 0.05F;
    const float tileW = cellPx_ - (2.0F * inset);
    const float radius = cellPx_ * 0.16F;
    const float edgeDrop = cellPx_ * 0.06F;
    const float numberSize = cellPx_ * 0.56F;
    const float emojiSize = cellPx_ * 0.62F;
    const bool lost = board_.state() == MineSweeperBoard::State::Lost;

    for (int r = 0; r < board_.height(); ++r) {
        for (int c = 0; c < board_.width(); ++c) {
            const MineSweeperBoard::Cell& cell = board_.at(r, c);
            const float x = originX_ + (static_cast<float>(c) * cellPx_) + inset;
            const float y = originY_ + (static_cast<float>(r) * cellPx_) + inset;
            const float cx = x + (tileW / 2.0F);
            const float cy = y + (tileW / 2.0F);

            if (!cell.revealed) {
                // Covered tile: a light card with a soft bottom edge.
                canvas.fillRoundedRect(x, y + edgeDrop, tileW, tileW, radius, theme().msTileEdge);
                canvas.fillRoundedRect(x, y, tileW, tileW, radius, theme().msTileCovered);
                if (cell.flagged) {
                    canvas.emojiCentered(kFlag, cx, cy, emojiSize);
                    if (lost && !cell.mine) {
                        // A wrong flag: cross it out in red.
                        const float a = tileW * 0.30F;
                        canvas.line(cx - a, cy - a, cx + a, cy + a, 8.0F, colors::hardRed);
                        canvas.line(cx - a, cy + a, cx + a, cy - a, 8.0F, colors::hardRed);
                    }
                }
                continue;
            }
            if (cell.mine) {
                if (cell.exploded) {
                    canvas.fillRoundedRect(x, y, tileW, tileW, radius, colors::hardRed);
                }
                canvas.emojiCentered(kBomb, cx, cy, emojiSize);
            } else if (cell.adjacent > 0) {
                canvas.textCentered(
                    std::to_string(cell.adjacent), cx, cy, numberSize,
                    colors::mineNumbers.at(static_cast<std::size_t>(cell.adjacent - 1)));
            }
        }
    }
}

void MineSweeperScene::drawBottomBar(Canvas& canvas) const {
    // Mines-left pill (💣 N).
    const float pillY = kBarCy - (kPillH / 2.0F);
    canvas.fillRoundedRect(kPillX, pillY, kPillW, kPillH, kPillH / 2.0F, theme().msPanel);
    canvas.emojiCentered(kBomb, kPillX + 50.0F, kBarCy, 44.0F);
    canvas.textCentered(std::to_string(board_.minesRemaining()), kPillX + 120.0F, kBarCy, 44.0F,
                        colors::white);

    // Dig / flag toggle: the active mode is tinted green, the other slate.
    canvas.fillRoundedRect(kToggleX, kBarCy - (kToggleH / 2.0F), kToggleW, kToggleH, 26.0F,
                           theme().msPanel);
    const Color digFill = flagging_ ? colors::mineToggleIdle : colors::mineToggleActive;
    const Color flagFill = flagging_ ? colors::mineToggleActive : colors::mineToggleIdle;
    canvas.fillRoundedRect(kDigX, kSquareY, kToggleSquare, kToggleSquare, 18.0F, digFill);
    canvas.fillRoundedRect(kFlagX, kSquareY, kToggleSquare, kToggleSquare, 18.0F, flagFill);
    canvas.emojiCentered(kShovel, kDigX + (kToggleSquare / 2.0F), kBarCy, 58.0F);
    canvas.emojiCentered(kFlag, kFlagX + (kToggleSquare / 2.0F), kBarCy, 58.0F);
}

void MineSweeperScene::drawOverlay(Canvas& canvas) const {
    canvas.fillRect(0.0F, 0.0F, layout::kWidthF, layout::kHeightF, colors::overlay);
    canvas.textCentered(resultText(), layout::kWidthF / 2.0F, 600.0F, 96.0F, colors::white);
    homeButton_.render(canvas);
    playAgainButton_.render(canvas);
}

void MineSweeperScene::render(Canvas& canvas) {
    canvas.clear(theme().msField);
    drawTopBar(canvas);
    drawBoard(canvas);
    drawBottomBar(canvas);
    if (phase_ == Phase::GameOver) {
        drawOverlay(canvas);
    }
}

} // namespace og
