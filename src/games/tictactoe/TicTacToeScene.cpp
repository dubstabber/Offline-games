#include "games/tictactoe/TicTacToeScene.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Theme.hpp"

#include <random>
#include <string>

namespace og {
namespace {

using Cell = TicTacToeBoard::Cell;

// ---- Back button (circular, top-left) -----------------------------------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;

// ---- Scoreboard panel ----------------------------------------------------
constexpr float kPanelW = 420.0F;
constexpr float kPanelH = 92.0F;
constexpr float kPanelX = (layout::kWidthF - kPanelW) / 2.0F;
constexpr float kPanelY = 64.0F;

// ---- Board: a `#` of four thick lines, no enclosing panel ---------------
constexpr float kGridSize = 480.0F;
constexpr float kGridX = (layout::kWidthF - kGridSize) / 2.0F;
constexpr float kGridY = 520.0F;
constexpr float kCell = kGridSize / 3.0F;
constexpr float kGridThickness = 16.0F;
constexpr float kGridOvershoot = 30.0F; // lines extend past the intersections

// ---- Game-over overlay row position -------------------------------------
constexpr float kButtonRowY = 760.0F;

constexpr float kBotThinkSeconds = 0.45F;

} // namespace

TicTacToeScene::TicTacToeScene(SceneManager& manager, Difficulty difficulty)
    : manager_(manager), bot_(std::random_device{}()), difficulty_(difficulty),
      backButton_(IconButton::Icon::Chevron, kBackCx, kBackCy, kBackRadius),
      overlay_(colors::youRed, colors::white, kButtonRowY) {
    backButton_.setOnTap([this] { manager_.pop(); });
    overlay_.setOnHome([this] { manager_.popToRoot(); });
    overlay_.setActionLabel("PLAY AGAIN");
    overlay_.setOnAction([this] { beginRound(); });
}

void TicTacToeScene::handleInput(const PointerEvent& event) {
    if (backButton_.handleInput(event)) {
        return;
    }
    if (phase_ == Phase::GameOver) {
        overlay_.handleInput(event);
        return;
    }
    // While the bot is "thinking" the board is locked.
    if (phase_ != Phase::PlayerTurn || event.phase != PointerEvent::Phase::Down) {
        return;
    }
    if (!hitTest(event, kGridX, kGridY, kGridSize, kGridSize)) {
        return;
    }
    const auto col = static_cast<std::size_t>((event.x - kGridX) / kCell);
    const auto row = static_cast<std::size_t>((event.y - kGridY) / kCell);
    const std::size_t index = (row * 3) + col;
    if (board_.at(index) != Cell::Empty) {
        return;
    }
    board_.place(index);
    if (board_.isOver()) {
        enterGameOver();
    } else {
        phase_ = Phase::BotThinking;
        botTimer_ = kBotThinkSeconds;
    }
}

void TicTacToeScene::update(float dtSeconds) {
    if (phase_ != Phase::BotThinking) {
        return;
    }
    botTimer_ -= dtSeconds;
    if (botTimer_ > 0.0F) {
        return;
    }
    board_.place(bot_.chooseMove(board_, difficulty_));
    if (board_.isOver()) {
        enterGameOver();
    } else {
        phase_ = Phase::PlayerTurn;
    }
}

void TicTacToeScene::beginRound() {
    board_.reset();
    botTimer_ = 0.0F;
    phase_ = Phase::PlayerTurn;
}

void TicTacToeScene::enterGameOver() {
    if (const auto winner = board_.winner()) {
        if (*winner == Cell::X) {
            ++youScore_;
        } else {
            ++botScore_;
        }
    }
    phase_ = Phase::GameOver;
}

std::string TicTacToeScene::resultText() const {
    if (const auto winner = board_.winner()) {
        return *winner == Cell::X ? "YOU WIN!" : "YOU LOST!";
    }
    return "DRAW!";
}

void TicTacToeScene::drawScoreboard(Canvas& canvas) const {
    // Difficulty tab, tucked behind the panel with its label peeking out above.
    // Drawn before the panel so its lower edge slips under the panel's top.
    constexpr float tabW = 176.0F;
    constexpr float tabH = 52.0F;
    const float tabX = (layout::kWidthF - tabW) / 2.0F;
    const float tabY = kPanelY - 34.0F;
    canvas.fillRoundedRect(tabX, tabY, tabW, tabH, 16.0F, theme().tttTab);
    // Label sits above the panel's top edge so the panel doesn't cover it.
    canvas.textCentered(label(difficulty_), layout::kWidthF / 2.0F, kPanelY - 14.0F, 24.0F,
                        colors::text);

    canvas.fillRoundedRect(kPanelX, kPanelY, kPanelW, kPanelH, 22.0F, theme().tttPanel);

    const float youCx = kPanelX + 84.0F;
    const float botCx = kPanelX + kPanelW - 84.0F;
    const float labelCy = kPanelY + 28.0F;
    const float scoreCy = kPanelY + 64.0F;

    canvas.textCentered("YOU", youCx, labelCy, 26.0F, colors::youRed);
    canvas.textCentered(std::to_string(youScore_), youCx, scoreCy, 40.0F, colors::youRed);
    canvas.textCentered("VS", layout::kWidthF / 2.0F, kPanelY + (kPanelH / 2.0F) + 2.0F, 44.0F,
                        colors::white);
    canvas.textCentered("BOT", botCx, labelCy, 26.0F, colors::botCyan);
    canvas.textCentered(std::to_string(botScore_), botCx, scoreCy, 40.0F, colors::botCyan);
}

void TicTacToeScene::drawGrid(Canvas& canvas) {
    for (int i = 1; i < 3; ++i) {
        const float offset = static_cast<float>(i) * kCell;
        // Vertical line (overshoots top and bottom).
        canvas.line(kGridX + offset, kGridY - kGridOvershoot, kGridX + offset,
                    kGridY + kGridSize + kGridOvershoot, kGridThickness, theme().tttGridLine);
        // Horizontal line (overshoots left and right).
        canvas.line(kGridX - kGridOvershoot, kGridY + offset, kGridX + kGridSize + kGridOvershoot,
                    kGridY + offset, kGridThickness, theme().tttGridLine);
    }
}

void TicTacToeScene::drawMarks(Canvas& canvas) const {
    for (std::size_t index = 0; index < TicTacToeBoard::kSize; ++index) {
        const Cell cell = board_.at(index);
        if (cell == Cell::Empty) {
            continue;
        }
        const std::size_t colIndex = index % 3;
        const std::size_t rowIndex = index / 3;
        const auto col = static_cast<float>(colIndex);
        const auto row = static_cast<float>(rowIndex);
        const float cx = kGridX + ((col + 0.5F) * kCell);
        const float cy = kGridY + ((row + 0.5F) * kCell);
        if (cell == Cell::X) {
            constexpr float arm = kCell * 0.30F;
            canvas.line(cx - arm, cy - arm, cx + arm, cy + arm, 18.0F, colors::youRed);
            canvas.line(cx - arm, cy + arm, cx + arm, cy - arm, 18.0F, colors::youRed);
        } else {
            constexpr float outer = kCell * 0.34F;
            canvas.fillCircle(cx, cy, outer, colors::botCyan);
            canvas.fillCircle(cx, cy, outer - 18.0F, theme().tttBg);
        }
    }
}

void TicTacToeScene::drawOverlay(Canvas& canvas) const {
    overlay_.render(canvas, resultText(), 560.0F, 96.0F);
}

void TicTacToeScene::render(Canvas& canvas) {
    canvas.clear(theme().tttBg);
    backButton_.render(canvas);
    drawScoreboard(canvas);
    drawGrid(canvas);
    drawMarks(canvas);
    if (phase_ == Phase::GameOver) {
        drawOverlay(canvas);
    }
}

} // namespace og
