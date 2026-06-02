#include "scenes/MenuScene.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "games/GameRegistry.hpp"
#include "scenes/DifficultySelectScene.hpp"

#include <memory>
#include <utility>

namespace og {
namespace {

constexpr float kCardW = 300.0F;
constexpr float kCardH = 360.0F;
constexpr float kGap = 32.0F;
constexpr float kGridW = (2.0F * kCardW) + kGap;
constexpr float kGridX = (layout::kWidthF - kGridW) / 2.0F;
constexpr float kFirstRowY = 220.0F;
constexpr Color kCardFill = rgb(40, 38, 52);
constexpr Color kCardPressed = rgb(58, 55, 74);

} // namespace

MenuScene::MenuScene(SceneManager& manager) : manager_(manager) {
    int i = 0;
    for (const GameInfo& game : gameRegistry()) {
        const int col = i % 2;
        const int row = i / 2;
        Card card;
        card.x = kGridX + (static_cast<float>(col) * (kCardW + kGap));
        card.y = kFirstRowY + (static_cast<float>(row) * (kCardH + kGap));
        card.w = kCardW;
        card.h = kCardH;
        card.info = game;
        cards_.push_back(std::move(card));
        ++i;
    }
}

void MenuScene::handleInput(const PointerEvent& event) {
    if (event.phase == PointerEvent::Phase::Down) {
        pressedIndex_ = -1;
        for (std::size_t i = 0; i < cards_.size(); ++i) {
            const Card& card = cards_[i];
            if (hitTest(event, card.x, card.y, card.w, card.h)) {
                pressedIndex_ = static_cast<int>(i);
                return;
            }
        }
        return;
    }
    // Phase::Up — open the game if the press both started and ended on it.
    const int pressed = pressedIndex_;
    pressedIndex_ = -1;
    if (pressed < 0) {
        return;
    }
    const Card& card = cards_[static_cast<std::size_t>(pressed)];
    if (hitTest(event, card.x, card.y, card.w, card.h)) {
        manager_.push(std::make_unique<DifficultySelectScene>(manager_, card.info));
    }
}

void MenuScene::update(float /*dtSeconds*/) {}

void MenuScene::render(Canvas& canvas) {
    canvas.clear(colors::cream);
    canvas.textCentered("OFFLINE GAMES", layout::kWidthF / 2.0F, 110.0F, 56.0F, colors::gridBlack);

    for (std::size_t i = 0; i < cards_.size(); ++i) {
        const Card& card = cards_[i];
        const bool pressed = std::cmp_equal(i, pressedIndex_);
        canvas.fillRoundedRect(card.x, card.y, card.w, card.h, 28.0F,
                               pressed ? kCardPressed : kCardFill);
        // Title (top-left) in the game's accent, with a short underline bar.
        canvas.text(card.info.title, card.x + 28.0F, card.y + 28.0F, 34.0F, card.info.accent,
                    Canvas::Align::Left);
        canvas.fillRoundedRect(card.x + 28.0F, card.y + 78.0F, 84.0F, 8.0F, 4.0F, card.info.accent);
        // Large emoji icon centered in the lower part of the card.
        canvas.textCentered(card.info.emoji, card.x + (card.w / 2.0F), card.y + (card.h * 0.62F),
                            150.0F, colors::text);
    }
}

} // namespace og
