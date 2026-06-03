#include "games/tapmatch/TapMatchScene.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Theme.hpp"

#include <array>
#include <cstddef>
#include <random>
#include <string>

namespace og {
namespace {

// ---- Back button (circular, top-left) — matches the other scenes -----------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;

// ---- Board: a fixed fine grid mapped to pixels (16x22 cells) ----------------
constexpr float kFineCell = 38.0F;       // one fine grid cell
constexpr float kTilePx = kFineCell * 2; // a tile footprint is 2x2 cells (76px)
constexpr float kBoardX = 56.0F;         // (720 - 16*38) / 2
constexpr float kBoardY = 200.0F;
constexpr float kTileInset = 5.0F; // gap between a tile's cell and its card
constexpr float kTileRadius = 16.0F;
constexpr float kEmojiPx = 46.0F; // fruit size on a ~66px card (leaves a grey margin)

// ---- Holder bar (7 slots, near the bottom) ---------------------------------
constexpr float kHolderX = 40.0F;
constexpr float kHolderY = 1150.0F;
constexpr float kHolderW = 640.0F;
constexpr float kHolderH = 124.0F;
constexpr float kHolderRadius = 28.0F;
constexpr float kSlotPad = 18.0F;
constexpr float kSlotGap = 10.0F;
constexpr float kSlotH = 90.0F;
constexpr float kHolderEmoji = 58.0F;

// ---- Game-over overlay buttons — same layout as Tic-Tac-Toe ----------------
constexpr float kButtonRowY = 760.0F;
constexpr float kHomeSize = 140.0F;
constexpr float kPlayAgainW = 360.0F;
constexpr float kButtonGap = 24.0F;
constexpr float kRowWidth = kHomeSize + kButtonGap + kPlayAgainW;
constexpr float kRowX = (layout::kWidthF - kRowWidth) / 2.0F;

[[nodiscard]] TapMatchBoard::GenParams paramsFor(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return {.iconVariety = 4,
                .layers = 2,
                .tileCount = 18,
                .holderBudget = 5,
                .gridWidth = 16,
                .gridHeight = 22,
                .clusters = 2};
    case Difficulty::Medium:
        return {.iconVariety = 6,
                .layers = 2,
                .tileCount = 36,
                .holderBudget = 6,
                .gridWidth = 16,
                .gridHeight = 22,
                .clusters = 4};
    case Difficulty::Hard:
        return {.iconVariety = 8,
                .layers = 3,
                .tileCount = 54,
                .holderBudget = 7,
                .gridWidth = 16,
                .gridHeight = 22,
                .clusters = 4};
    }
    return {.iconVariety = 6,
            .layers = 2,
            .tileCount = 36,
            .holderBudget = 6,
            .gridWidth = 16,
            .gridHeight = 22,
            .clusters = 4};
}

// Icon index -> a fruit emoji (UTF-8 bytes). A board uses the first `iconVariety`
// of these; the rest cover up to TapMatchBoard::kMaxIcons.
[[nodiscard]] const char* emojiFor(int icon) {
    static constexpr std::array<const char*, TapMatchBoard::kMaxIcons> kIcons = {
        "\xF0\x9F\x8D\x93", // 🍓 strawberry
        "\xF0\x9F\x8D\x8C", // 🍌 banana
        "\xF0\x9F\xAB\x90", // 🫐 blueberries
        "\xF0\x9F\x8D\x89", // 🍉 watermelon
        "\xF0\x9F\x8D\x91", // 🍑 peach
        "\xF0\x9F\xA5\x9D", // 🥝 kiwi
        "\xF0\x9F\x8D\x90", // 🍐 pear
        "\xF0\x9F\x8D\x8F", // 🍏 green apple
        "\xF0\x9F\x8D\x87", // 🍇 grapes
        "\xF0\x9F\x8D\x8A", // 🍊 tangerine
        "\xF0\x9F\x8D\x92", // 🍒 cherries
        "\xF0\x9F\x8D\x8D", // 🍍 pineapple
        "\xF0\x9F\x8D\x8B", // 🍋 lemon
        "\xF0\x9F\x8D\x8E", // 🍎 red apple
        "\xF0\x9F\xA5\xAD", // 🥭 mango
        "\xF0\x9F\x8D\x88", // 🍈 melon
        "\xF0\x9F\xA5\xA5", // 🥥 coconut
        "\xF0\x9F\xA5\x91", // 🥑 avocado
        "\xF0\x9F\x8D\x85", // 🍅 tomato
        "\xF0\x9F\xAB\x92", // 🫒 olive
    };
    if (icon < 0 || icon >= TapMatchBoard::kMaxIcons) {
        return "";
    }
    return kIcons.at(static_cast<std::size_t>(icon));
}

} // namespace

TapMatchScene::TapMatchScene(SceneManager& manager, Difficulty difficulty)
    : manager_(manager), difficulty_(difficulty),
      board_(paramsFor(difficulty), std::random_device{}()),
      homeButton_("\xF0\x9F\x8F\xA0", kRowX, kButtonRowY, kHomeSize, kHomeSize), // 🏠
      playAgainButton_("PLAY AGAIN", kRowX + kHomeSize + kButtonGap, kButtonRowY, kPlayAgainW,
                       kHomeSize) {
    homeButton_.setColors(colors::white, colors::panelBrown);
    homeButton_.setOnTap([this] { manager_.popToRoot(); });
    playAgainButton_.setColors(color(difficulty_), colors::white);
    playAgainButton_.setOnTap([this] { beginRound(); });
}

bool TapMatchScene::handleBackButton(const PointerEvent& event) {
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

int TapMatchScene::pickAccessibleAt(float px, float py) const {
    int best = -1;
    int bestLayer = -1;
    for (const auto& tile : board_.tiles()) {
        if (tile.removed) {
            continue;
        }
        const float x = kBoardX + (static_cast<float>(tile.x) * kFineCell);
        const float y = kBoardY + (static_cast<float>(tile.y) * kFineCell);
        if (px >= x && px < x + kTilePx && py >= y && py < y + kTilePx && tile.layer > bestLayer) {
            best = tile.id;
            bestLayer = tile.layer;
        }
    }
    return (best >= 0 && board_.isAccessible(best)) ? best : -1;
}

void TapMatchScene::handleInput(const PointerEvent& event) {
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
    if (event.phase != PointerEvent::Phase::Down) {
        return;
    }
    const int id = pickAccessibleAt(event.x, event.y);
    if (id < 0) {
        return;
    }
    board_.tapTile(id);
    if (board_.isOver()) {
        enterGameOver();
    }
}

void TapMatchScene::update(float /*dtSeconds*/) {
    // The model is instant; nothing to animate yet.
}

void TapMatchScene::beginRound() {
    board_ = TapMatchBoard(paramsFor(difficulty_), std::random_device{}());
    phase_ = Phase::Playing;
}

void TapMatchScene::enterGameOver() {
    phase_ = Phase::GameOver;
}

std::string TapMatchScene::statusText() const {
    return std::to_string(board_.remaining()) + " left";
}

const char* TapMatchScene::resultText() const {
    return board_.result() == TapMatchBoard::Result::Won ? "YOU WIN!" : "STACK FULL!";
}

void TapMatchScene::drawBackButton(Canvas& canvas) {
    canvas.fillCircle(kBackCx, kBackCy, kBackRadius, theme().backCircle);
    canvas.line(kBackCx + 12.0F, kBackCy - 24.0F, kBackCx - 14.0F, kBackCy, 14.0F, theme().chevron);
    canvas.line(kBackCx - 14.0F, kBackCy, kBackCx + 12.0F, kBackCy + 24.0F, 14.0F, theme().chevron);
}

void TapMatchScene::drawTopBar(Canvas& canvas) const {
    drawBackButton(canvas);
    canvas.textCentered("Tap Match", layout::kWidthF / 2.0F, 92.0F, 52.0F, theme().primaryText);
    canvas.textCentered(statusText(), layout::kWidthF / 2.0F, 150.0F, 28.0F, theme().tmStatusText);
}

void TapMatchScene::drawBoard(Canvas& canvas) const {
    // tiles() is ordered by ascending layer, so iterating in order paints higher
    // (covering) tiles on top of the ones they sit over.
    for (const auto& tile : board_.tiles()) {
        if (tile.removed) {
            continue;
        }
        const float x = kBoardX + (static_cast<float>(tile.x) * kFineCell);
        const float y = kBoardY + (static_cast<float>(tile.y) * kFineCell);
        const bool accessible = board_.isAccessible(tile.id);
        const float cardW = kTilePx - (2.0F * kTileInset);
        // A grey border (slightly larger) separates overlapping tiles, then the
        // card itself (white & raised when free, grey when covered).
        canvas.fillRoundedRect(x + kTileInset - 2.0F, y + kTileInset - 2.0F, cardW + 4.0F,
                               cardW + 4.0F, kTileRadius + 2.0F, theme().tmTileEdge);
        canvas.fillRoundedRect(x + kTileInset, y + kTileInset, cardW, cardW, kTileRadius,
                               accessible ? theme().tmTileLight : theme().tmTileDim);
        canvas.emojiCentered(emojiFor(tile.icon), x + (kTilePx / 2.0F), y + (kTilePx / 2.0F),
                             kEmojiPx);
    }
}

void TapMatchScene::drawHolder(Canvas& canvas) const {
    canvas.fillRoundedRect(kHolderX, kHolderY, kHolderW, kHolderH, kHolderRadius, theme().tmHolder);
    const float slotW = (kHolderW - (2.0F * kSlotPad) - (6.0F * kSlotGap)) / 7.0F;
    const float slotY = kHolderY + ((kHolderH - kSlotH) / 2.0F);
    for (int i = 0; i < TapMatchBoard::kHolderCapacity; ++i) {
        const float sx = kHolderX + kSlotPad + (static_cast<float>(i) * (slotW + kSlotGap));
        canvas.fillRoundedRect(sx, slotY, slotW, kSlotH, 16.0F, theme().tmSlot);
    }
    const std::vector<int>& held = board_.holder();
    for (std::size_t i = 0;
         i < held.size() && i < static_cast<std::size_t>(TapMatchBoard::kHolderCapacity); ++i) {
        const float sx = kHolderX + kSlotPad + (static_cast<float>(i) * (slotW + kSlotGap));
        canvas.emojiCentered(emojiFor(held.at(i)), sx + (slotW / 2.0F), slotY + (kSlotH / 2.0F),
                             kHolderEmoji);
    }
}

void TapMatchScene::drawOverlay(Canvas& canvas) const {
    canvas.fillRect(0.0F, 0.0F, layout::kWidthF, layout::kHeightF, colors::overlay);
    canvas.textCentered(resultText(), layout::kWidthF / 2.0F, 560.0F, 92.0F, colors::white);
    homeButton_.render(canvas);
    playAgainButton_.render(canvas);
}

void TapMatchScene::render(Canvas& canvas) {
    canvas.clear(theme().tmBg);
    drawTopBar(canvas);
    drawBoard(canvas);
    drawHolder(canvas);
    if (phase_ == Phase::GameOver) {
        drawOverlay(canvas);
    }
}

} // namespace og
