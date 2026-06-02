#include "scenes/MenuScene.hpp"

#include "core/Canvas.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "games/GameRegistry.hpp"

namespace og {
namespace {

constexpr float kEntryWidth = 600.0F;
constexpr float kEntryHeight = 150.0F;
constexpr float kEntryGap = 40.0F;
constexpr float kFirstEntryY = 360.0F;

} // namespace

MenuScene::MenuScene(SceneManager& manager) : manager_(manager) {
    const float x = (layout::kWidthF - kEntryWidth) / 2.0F;
    float y = kFirstEntryY;
    for (const GameInfo& game : gameRegistry()) {
        Button button(game.emoji + "  " + game.title, x, y, kEntryWidth, kEntryHeight);
        button.setColors(colors::surface, colors::text);
        // Capture by value so the button keeps working regardless of registry
        // lifetime; `create` is a small std::function copy.
        button.setOnTap([this, create = game.create] {
            if (create) {
                manager_.push(create(manager_));
            }
        });
        entries_.push_back(std::move(button));
        y += kEntryHeight + kEntryGap;
    }
}

void MenuScene::handleInput(const PointerEvent& event) {
    for (Button& entry : entries_) {
        if (entry.handleInput(event)) {
            return;
        }
    }
}

void MenuScene::update(float /*dtSeconds*/) {}

void MenuScene::render(Canvas& canvas) {
    canvas.text("Offline Games", layout::kWidthF / 2.0F, 150.0F, 72.0F, colors::text,
                Canvas::Align::Center);
    canvas.text("\xF0\x9F\x8E\xAE Pick a game", layout::kWidthF / 2.0F, 250.0F, 40.0F,
                colors::textMuted, Canvas::Align::Center); // 🎮
    for (const Button& entry : entries_) {
        entry.render(canvas);
    }
}

} // namespace og
