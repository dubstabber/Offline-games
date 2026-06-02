#include "scenes/MenuScene.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "games/GameRegistry.hpp"
#include "scenes/DifficultySelectScene.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>

namespace og {
namespace {

// ---- Fixed top bar -------------------------------------------------------
constexpr float kTopBarH = 96.0F;
constexpr Color kBarLine = rgb(226, 206, 180); // separator under the white bar

// ---- Card grid (laid out in content space, scrolled by scrollY_) ---------
constexpr float kCardW = 300.0F;
constexpr float kCardH = 360.0F;
constexpr float kGap = 32.0F;
constexpr float kGridW = (2.0F * kCardW) + kGap;
constexpr float kGridX = (layout::kWidthF - kGridW) / 2.0F;
constexpr float kFirstRowY = kTopBarH + 36.0F; // top of the first row, content space
constexpr float kBottomPad = 36.0F;            // breathing room past the last row
constexpr Color kCardFill = rgb(40, 38, 52);
constexpr Color kCardPressed = rgb(58, 55, 74);

// A press may drift this far and still count as a tap; beyond it, it's a scroll.
constexpr float kTapSlop = 18.0F;

// Inertial ("fling") scrolling: after the finger lifts, the grid keeps gliding
// and decelerates. The velocity tracks the finger while dragging, then decays
// exponentially — so a faster swipe starts faster and coasts farther/longer.
constexpr float kVelSmoothing = 0.5F;   // how quickly tracked velocity follows the finger
constexpr float kFlingDecay = 6.0F;     // per-second exponential velocity decay
constexpr float kMinFlingSpeed = 24.0F; // px/s below which the glide just stops

// Total content height for the given number of cards (used to clamp scrolling).
[[nodiscard]] float contentHeight(std::size_t cardCount) {
    const std::size_t rows = (cardCount + 1) / 2; // two cards per row, rounded up
    return kFirstRowY + (static_cast<float>(rows) * (kCardH + kGap)) - kGap + kBottomPad;
}

// The "SOON" pill drawn on placeholder cards (no game behind them yet).
void drawSoonBadge(Canvas& canvas, float cardX, float cardY, Color accent) {
    constexpr float kW = 120.0F;
    constexpr float kH = 50.0F;
    const float x = cardX + 24.0F;
    const float y = cardY + kCardH - kH - 24.0F;
    canvas.fillRoundedRect(x, y, kW, kH, kH / 2.0F, accent);
    canvas.textCentered("SOON", x + (kW / 2.0F), y + (kH / 2.0F), 26.0F, colors::white);
}

void drawTopBar(Canvas& canvas) {
    canvas.fillRect(0.0F, 0.0F, layout::kWidthF, kTopBarH, colors::white);
    canvas.fillRect(0.0F, kTopBarH, layout::kWidthF, 3.0F, kBarLine);
    // Placeholder menu button: three short rounded bars (a "hamburger") at left.
    constexpr float kLineW = 56.0F;
    constexpr float kLineH = 9.0F;
    constexpr float kLineGap = 12.0F;
    constexpr float kX = 40.0F;
    float y = (kTopBarH - ((3.0F * kLineH) + (2.0F * kLineGap))) / 2.0F;
    for (int i = 0; i < 3; ++i) {
        canvas.fillRoundedRect(kX, y, kLineW, kLineH, kLineH / 2.0F, colors::menuPink);
        y += kLineH + kLineGap;
    }
}

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
    maxScroll_ = std::max(0.0F, contentHeight(cards_.size()) - layout::kHeightF);
}

int MenuScene::cardAt(const PointerEvent& event) const {
    for (std::size_t i = 0; i < cards_.size(); ++i) {
        const Card& card = cards_[i];
        if (hitTest(event, card.x, card.y - scrollY_, card.w, card.h)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void MenuScene::openCard(const Card& card) {
    if (card.info.create) {
        manager_.push(std::make_unique<DifficultySelectScene>(manager_, card.info));
    }
    // Placeholder cards (no factory) have no game to open yet.
}

void MenuScene::handleInput(const PointerEvent& event) {
    switch (event.phase) {
    case PointerEvent::Phase::Down:
        // Any touch halts an in-progress glide (like Android: catch to stop).
        velocityY_ = 0.0F;
        dragAccumY_ = 0.0F;
        // The fixed top bar swallows its own touches; the grid never scrolls
        // from there (and the placeholder menu button does nothing for now).
        if (event.y < kTopBarH) {
            gestureActive_ = false;
            pressedIndex_ = -1;
            return;
        }
        gestureActive_ = true;
        gestureScrolled_ = false;
        pressStartY_ = event.y;
        lastPointerY_ = event.y;
        pressedIndex_ = cardAt(event);
        return;
    case PointerEvent::Phase::Move: {
        if (!gestureActive_) {
            return;
        }
        const float before = scrollY_;
        scrollY_ = std::clamp(scrollY_ - (event.y - lastPointerY_), 0.0F, maxScroll_);
        dragAccumY_ += scrollY_ - before; // actual (clamp-limited) movement, for velocity
        lastPointerY_ = event.y;
        // Once the finger has travelled past the slop it's a scroll, not a tap:
        // drop the pending press so releasing won't open a card.
        if (std::abs(event.y - pressStartY_) > kTapSlop) {
            gestureScrolled_ = true;
            pressedIndex_ = -1;
        }
        return;
    }
    case PointerEvent::Phase::Up:
        if (gestureActive_ && !gestureScrolled_ && pressedIndex_ >= 0 &&
            cardAt(event) == pressedIndex_) {
            openCard(cards_[static_cast<std::size_t>(pressedIndex_)]);
        }
        // A tap (no real drag) must never fling, including on return to the menu.
        if (!gestureScrolled_) {
            velocityY_ = 0.0F;
            dragAccumY_ = 0.0F;
        }
        gestureActive_ = false;
        pressedIndex_ = -1;
        return;
    }
}

void MenuScene::update(float dtSeconds) {
    if (dtSeconds <= 0.0F) {
        return;
    }
    // While dragging, keep a smoothed estimate of the finger's velocity so the
    // value at release reflects the recent swipe (a pause bleeds it back to 0).
    if (gestureActive_) {
        const float instant = dragAccumY_ / dtSeconds;
        velocityY_ += (instant - velocityY_) * kVelSmoothing;
        dragAccumY_ = 0.0F;
        return;
    }
    // The release frame still carries the last drag movement: fold it in once.
    if (dragAccumY_ != 0.0F) {
        const float instant = dragAccumY_ / dtSeconds;
        velocityY_ += (instant - velocityY_) * kVelSmoothing;
        dragAccumY_ = 0.0F;
    }
    // Glide: advance by the velocity, then decay it; stop at the edges or once slow.
    if (std::abs(velocityY_) < kMinFlingSpeed) {
        velocityY_ = 0.0F;
        return;
    }
    scrollY_ = std::clamp(scrollY_ + (velocityY_ * dtSeconds), 0.0F, maxScroll_);
    velocityY_ *= std::exp(-kFlingDecay * dtSeconds);
    if (scrollY_ <= 0.0F || scrollY_ >= maxScroll_) {
        velocityY_ = 0.0F;
    }
}

void MenuScene::render(Canvas& canvas) {
    canvas.clear(colors::cream);

    // Cards scroll under the bar; they're drawn first so the opaque bar (drawn
    // last) covers anything that has scrolled up into its area.
    for (std::size_t i = 0; i < cards_.size(); ++i) {
        const Card& card = cards_[i];
        const float y = card.y - scrollY_;
        if (y + card.h < 0.0F || y > layout::kHeightF) {
            continue; // fully off-screen
        }
        const bool pressed = std::cmp_equal(i, pressedIndex_);
        canvas.fillRoundedRect(card.x, y, card.w, card.h, 28.0F,
                               pressed ? kCardPressed : kCardFill);
        // Title (top-left) in the game's accent, with a short underline bar.
        canvas.text(card.info.title, card.x + 28.0F, y + 28.0F, 34.0F, card.info.accent,
                    Canvas::Align::Left);
        canvas.fillRoundedRect(card.x + 28.0F, y + 78.0F, 84.0F, 8.0F, 4.0F, card.info.accent);
        // Large emoji icon centered in the lower part of the card.
        canvas.textCentered(card.info.emoji, card.x + (card.w / 2.0F), y + (card.h * 0.62F), 150.0F,
                            colors::text);
        if (!card.info.create) {
            drawSoonBadge(canvas, card.x, y, card.info.accent);
        }
    }

    drawTopBar(canvas);
}

} // namespace og
