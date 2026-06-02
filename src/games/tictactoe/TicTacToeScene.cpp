#include "games/tictactoe/TicTacToeScene.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"

namespace og {
namespace {

// Board geometry in logical canvas pixels.
constexpr float kGridSize = 600.0F;
constexpr float kGridX = (layout::kWidthF - kGridSize) / 2.0F;
constexpr float kGridY = 440.0F;
constexpr float kCell = kGridSize / 3.0F;

// UTF-8 for the player marks. Kept here so the logic class stays free of any
// presentation detail.
constexpr const char* kMarkX = "\xE2\x9D\x8C"; // ❌
constexpr const char* kMarkO = "\xE2\xAD\x95"; // ⭕

const char* markGlyph(TicTacToeBoard::Cell cell) {
    switch (cell) {
    case TicTacToeBoard::Cell::X:
        return kMarkX;
    case TicTacToeBoard::Cell::O:
        return kMarkO;
    case TicTacToeBoard::Cell::Empty:
        return "";
    }
    return "";
}

} // namespace

TicTacToeScene::TicTacToeScene(SceneManager& manager)
    : manager_(manager), backButton_("\xE2\x86\x90 Back", 40.0F, 48.0F, 220.0F, 96.0F),
      newGameButton_("New Game", (layout::kWidthF - 400.0F) / 2.0F, 1180.0F, 400.0F, 120.0F) {
    backButton_.setColors(colors::surface, colors::text);
    backButton_.setOnTap([this] { manager_.pop(); });
    newGameButton_.setColors(colors::surfaceAlt, colors::text);
    newGameButton_.setOnTap([this] { board_.reset(); });
}

void TicTacToeScene::handleInput(const PointerEvent& event) {
    if (backButton_.handleInput(event)) {
        return;
    }
    if (newGameButton_.handleInput(event)) {
        return;
    }
    // A tap (press) inside the grid places a mark for the current player.
    if (event.phase != PointerEvent::Phase::Down || board_.isOver()) {
        return;
    }
    if (!hitTest(event, kGridX, kGridY, kGridSize, kGridSize)) {
        return;
    }
    const auto col = static_cast<std::size_t>((event.x - kGridX) / kCell);
    const auto row = static_cast<std::size_t>((event.y - kGridY) / kCell);
    board_.place((row * 3) + col);
}

void TicTacToeScene::update(float /*dtSeconds*/) {}

std::string TicTacToeScene::statusText() const {
    if (const auto winner = board_.winner()) {
        return std::string(markGlyph(*winner)) + " wins!";
    }
    if (board_.isDraw()) {
        return "It's a draw \xF0\x9F\xA4\x9D"; // 🤝
    }
    return std::string("Turn: ") + markGlyph(board_.currentPlayer());
}

void TicTacToeScene::render(Canvas& canvas) {
    canvas.text("Tic-Tac-Toe", layout::kWidthF / 2.0F, 180.0F, 64.0F, colors::text,
                Canvas::Align::Center);
    canvas.textCentered(statusText(), layout::kWidthF / 2.0F, 340.0F, 52.0F, colors::accent);

    // Board background panel.
    canvas.fillRoundedRect(kGridX - 16.0F, kGridY - 16.0F, kGridSize + 32.0F, kGridSize + 32.0F,
                           28.0F, colors::surface);

    // Grid lines.
    for (int i = 1; i < 3; ++i) {
        const float offset = static_cast<float>(i) * kCell;
        canvas.line(kGridX + offset, kGridY, kGridX + offset, kGridY + kGridSize, 8.0F,
                    colors::line);
        canvas.line(kGridX, kGridY + offset, kGridX + kGridSize, kGridY + offset, 8.0F,
                    colors::line);
    }

    // Marks.
    for (std::size_t index = 0; index < TicTacToeBoard::kSize; ++index) {
        const char* glyph = markGlyph(board_.at(index));
        if (*glyph == '\0') {
            continue;
        }
        const std::size_t colIndex = index % 3;
        const std::size_t rowIndex = index / 3;
        const auto col = static_cast<float>(colIndex);
        const auto row = static_cast<float>(rowIndex);
        const float cx = kGridX + ((col + 0.5F) * kCell);
        const float cy = kGridY + ((row + 0.5F) * kCell);
        canvas.textCentered(glyph, cx, cy, kCell * 0.62F, colors::text);
    }

    backButton_.render(canvas);
    newGameButton_.render(canvas);
}

} // namespace og
