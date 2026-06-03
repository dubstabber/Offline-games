#include "games/tapmatch/TapMatchScene.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Theme.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

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

// ---- Animation tuning (reimplements the original's feel) -------------------
// Fly-to-holder time grows with travel distance (like the original's linear,
// distance-scaled tween), clamped to a snappy range.
constexpr float kFlyPxPerSec = 2600.0F;
constexpr float kFlyMin = 0.16F;
constexpr float kFlyMax = 0.40F;
constexpr float kSlideRate = 19.0F; // holder reflow: exp slide toward each slot
constexpr float kClearDur = 0.42F;  // matched fruit: pop then shrink-and-vanish
// Board intro: tiles fly in from the corners, staggered by layer.
constexpr float kIntroDur = 0.30F;
constexpr float kLayerStagger = 0.09F;
constexpr float kIntroDist = 460.0F;
// Match burst.
constexpr int kSparkCount = 8;
constexpr float kSparkSpeed = 430.0F;
constexpr float kSparkLife = 0.36F;
constexpr float kTwoPi = 6.2831853F;

// Holder slot geometry (the 7 fixed slots the held fruit snap to).
constexpr int kSlots = TapMatchBoard::kHolderCapacity;
constexpr float kSlotW =
    (kHolderW - (2.0F * kSlotPad) - (static_cast<float>(kSlots - 1) * kSlotGap)) /
    static_cast<float>(kSlots);
constexpr float kHolderRowCy = kHolderY + (kHolderH / 2.0F);

[[nodiscard]] float slotCenterX(int index) {
    return kHolderX + kSlotPad + (static_cast<float>(index) * (kSlotW + kSlotGap)) +
           (kSlotW / 2.0F);
}

[[nodiscard]] float clampUnit(float v) {
    return std::clamp(v, 0.0F, 1.0F);
}
[[nodiscard]] float lerp(float a, float b, float t) {
    return a + ((b - a) * t);
}
[[nodiscard]] float easeOutCubic(float t) {
    const float u = 1.0F - t;
    return 1.0F - (u * u * u);
}
[[nodiscard]] float easeInCubic(float t) {
    return t * t * t;
}
[[nodiscard]] float easeOutQuad(float t) {
    return 1.0F - ((1.0F - t) * (1.0F - t));
}
[[nodiscard]] float easeOutBack(float t) {
    constexpr float c1 = 1.70158F;
    constexpr float c3 = c1 + 1.0F;
    const float u = t - 1.0F;
    return 1.0F + (c3 * u * u * u) + (c1 * u * u);
}

[[nodiscard]] float flyDuration(float x0, float y0, float x1, float y1) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    return std::clamp(std::sqrt((dx * dx) + (dy * dy)) / kFlyPxPerSec, kFlyMin, kFlyMax);
}

// Scale of a matched fruit `p` of the way through its clear: a quick pop to 1.3
// then a shrink to 0 (echoes the original's pop-then-shrink solve clip).
[[nodiscard]] float clearScale(float p) {
    if (p < 0.25F) {
        return 1.0F + (0.3F * easeOutCubic(p / 0.25F));
    }
    return 1.3F * (1.0F - easeInCubic((p - 0.25F) / 0.75F));
}

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
    resetFx();
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
    if (introActive() || event.phase != PointerEvent::Phase::Down) {
        return; // the board is still flying in, or this isn't a press
    }
    const int id = pickAccessibleAt(event.x, event.y);
    if (id < 0) {
        return;
    }
    startTap(id);
}

void TapMatchScene::update(float dtSeconds) {
    advanceFx(dtSeconds);
}

void TapMatchScene::beginRound() {
    board_ = TapMatchBoard(paramsFor(difficulty_), std::random_device{}());
    phase_ = Phase::Playing;
    resetFx();
}

void TapMatchScene::enterGameOver() {
    phase_ = Phase::GameOver;
}

void TapMatchScene::resetFx() {
    tray_.clear();
    flying_.clear();
    clearing_.clear();
    sparks_.clear();
    introClock_ = 0.0F;
    nextUid_ = 1;
    wonPending_ = false;
    int maxLayer = 0;
    for (const auto& tile : board_.tiles()) {
        maxLayer = std::max(maxLayer, tile.layer);
    }
    introEnd_ = (static_cast<float>(maxLayer) * kLayerStagger) + kIntroDur;
}

bool TapMatchScene::introActive() const {
    return introClock_ < introEnd_;
}

int TapMatchScene::trayIndexOf(int uid) const {
    for (std::size_t i = 0; i < tray_.size(); ++i) {
        if (tray_.at(i).uid == uid) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void TapMatchScene::spawnSparks(float x, float y) {
    for (int i = 0; i < kSparkCount; ++i) {
        const float angle = (static_cast<float>(i) / static_cast<float>(kSparkCount)) * kTwoPi;
        sparks_.push_back({.x = x,
                           .y = y,
                           .vx = std::cos(angle) * kSparkSpeed,
                           .vy = std::sin(angle) * kSparkSpeed,
                           .age = 0.0F,
                           .life = kSparkLife});
    }
}

// Tap a board tile and spawn the fruit's flight to the holder. The board model
// updates instantly; tray_/clearing_ animate that change. board_.holder() is the
// source of truth for grouping, so it's queried *before* the tap.
void TapMatchScene::startTap(int id) {
    const TapMatchBoard::Tile& tile = board_.tiles().at(static_cast<std::size_t>(id));
    const int icon = tile.icon;
    const float cx = kBoardX + (static_cast<float>(tile.x) * kFineCell) + (kTilePx / 2.0F);
    const float cy = kBoardY + (static_cast<float>(tile.y) * kFineCell) + (kTilePx / 2.0F);

    int preCount = 0;
    for (const int held : board_.holder()) {
        if (held == icon) {
            ++preCount;
        }
    }
    const bool willTriple = (preCount == TapMatchBoard::kGroupSize - 1);

    if (!board_.tapTile(id)) {
        return;
    }
    if (board_.result() == TapMatchBoard::Result::Lost) {
        enterGameOver();
        return;
    }

    // Destination slot = where this icon groups in the tray (first match, or end).
    int slot = static_cast<int>(tray_.size());
    for (std::size_t i = 0; i < tray_.size(); ++i) {
        if (tray_.at(i).icon == icon) {
            slot = static_cast<int>(i);
            break;
        }
    }
    const float destX = slotCenterX(slot);

    FlyTile fly{};
    fly.icon = icon;
    fly.x0 = cx;
    fly.y0 = cy;
    fly.size0 = kEmojiPx;
    fly.dur = flyDuration(cx, cy, destX, kHolderRowCy);

    if (willTriple) {
        // Park the two existing same-icon fruit as clears that hold until the 3rd
        // lands, collapse the tray now, and let the 3rd burst on arrival.
        float sumX = destX;
        int count = 1;
        for (auto it = tray_.begin(); it != tray_.end();) {
            if (it->icon != icon) {
                ++it;
                continue;
            }
            clearing_.push_back(
                {.icon = icon, .x = it->x, .y = kHolderRowCy, .wait = fly.dur, .age = 0.0F});
            sumX += it->x;
            ++count;
            std::erase_if(flying_,
                          [uid = it->uid](const FlyTile& f) { return !f.triple && f.uid == uid; });
            it = tray_.erase(it);
        }
        fly.triple = true;
        fly.targetX = sumX / static_cast<float>(count);
        fly.targetY = kHolderRowCy;
    } else {
        const TrayTile arriving{.uid = nextUid_++, .icon = icon, .x = destX, .arriving = true};
        tray_.insert(tray_.begin() + slot, arriving);
        fly.uid = arriving.uid;
    }
    flying_.push_back(fly);

    if (board_.result() == TapMatchBoard::Result::Won) {
        wonPending_ = true; // defer the overlay until the last match finishes
    }
}

void TapMatchScene::advanceFx(float dtSeconds) {
    if (dtSeconds <= 0.0F) {
        return;
    }
    introClock_ += dtSeconds;

    // Flights: advance, and on landing either settle a tray fruit or burst a match.
    for (auto it = flying_.begin(); it != flying_.end();) {
        it->t += dtSeconds;
        if (it->t < it->dur) {
            ++it;
            continue;
        }
        if (it->triple) {
            clearing_.push_back(
                {.icon = it->icon, .x = it->targetX, .y = it->targetY, .wait = 0.0F, .age = 0.0F});
            spawnSparks(it->targetX, it->targetY);
        } else {
            const int idx = trayIndexOf(it->uid);
            if (idx >= 0) {
                tray_.at(static_cast<std::size_t>(idx)).arriving = false;
            }
        }
        it = flying_.erase(it);
    }

    // Holder reflow: slide each fruit toward its slot.
    const float slide = 1.0F - std::exp(-kSlideRate * dtSeconds);
    for (std::size_t i = 0; i < tray_.size(); ++i) {
        TrayTile& tile = tray_.at(i);
        tile.x += (slotCenterX(static_cast<int>(i)) - tile.x) * slide;
    }

    // Clears: hold (until the 3rd lands), then pop and shrink; drop when done.
    for (auto it = clearing_.begin(); it != clearing_.end();) {
        it->age += dtSeconds;
        if ((it->age - it->wait) >= kClearDur) {
            it = clearing_.erase(it);
        } else {
            ++it;
        }
    }

    // Sparks: fly out and decelerate, then expire.
    const float damp = std::exp(-3.0F * dtSeconds);
    for (auto it = sparks_.begin(); it != sparks_.end();) {
        it->age += dtSeconds;
        if (it->age >= it->life) {
            it = sparks_.erase(it);
            continue;
        }
        it->x += it->vx * dtSeconds;
        it->y += it->vy * dtSeconds;
        it->vx *= damp;
        it->vy *= damp;
        ++it;
    }

    if (wonPending_ && flying_.empty() && clearing_.empty()) {
        wonPending_ = false;
        enterGameOver();
    }
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
    const bool intro = introActive();
    // tiles() is ordered by ascending layer, so iterating in order paints higher
    // (covering) tiles on top of the ones they sit over.
    for (const auto& tile : board_.tiles()) {
        if (tile.removed) {
            continue;
        }
        float cx = kBoardX + (static_cast<float>(tile.x) * kFineCell) + (kTilePx / 2.0F);
        float cy = kBoardY + (static_cast<float>(tile.y) * kFineCell) + (kTilePx / 2.0F);
        float scale = 1.0F;
        if (intro) {
            // Fly in from a corner, staggered by layer (later layers land later).
            const float start = static_cast<float>(tile.layer) * kLayerStagger;
            if (introClock_ < start) {
                continue; // not spawned yet
            }
            const float e = easeOutCubic(clampUnit((introClock_ - start) / kIntroDur));
            const float off = kIntroDist * (1.0F - e);
            cx += ((tile.id & 1) != 0 ? off : -off);
            cy += ((tile.id & 2) != 0 ? off : -off);
            scale = lerp(0.5F, 1.0F, e);
        }
        const bool accessible = board_.isAccessible(tile.id);
        const float cardW = (kTilePx - (2.0F * kTileInset)) * scale;
        // A grey border (slightly larger) separates overlapping tiles, then the
        // card itself (white & raised when free, grey when covered).
        canvas.fillRoundedRect(cx - ((cardW + 4.0F) / 2.0F), cy - ((cardW + 4.0F) / 2.0F),
                               cardW + 4.0F, cardW + 4.0F, (kTileRadius + 2.0F) * scale,
                               theme().tmTileEdge);
        canvas.fillRoundedRect(cx - (cardW / 2.0F), cy - (cardW / 2.0F), cardW, cardW,
                               kTileRadius * scale,
                               accessible ? theme().tmTileLight : theme().tmTileDim);
        canvas.emojiCentered(emojiFor(tile.icon), cx, cy, kEmojiPx * scale);
    }
}

void TapMatchScene::drawHolder(Canvas& canvas) {
    canvas.fillRoundedRect(kHolderX, kHolderY, kHolderW, kHolderH, kHolderRadius, theme().tmHolder);
    const float slotY = kHolderY + ((kHolderH - kSlotH) / 2.0F);
    for (int i = 0; i < kSlots; ++i) {
        canvas.fillRoundedRect(slotCenterX(i) - (kSlotW / 2.0F), slotY, kSlotW, kSlotH, 16.0F,
                               theme().tmSlot);
    }
}

// The held fruit (animated x as the tray reflows) and the matched fruit popping
// out. Drawn above the holder slots but below the in-flight fruit.
void TapMatchScene::drawTray(Canvas& canvas) const {
    for (const TrayTile& tile : tray_) {
        if (tile.arriving) {
            continue; // still drawn by its flight until it lands
        }
        canvas.emojiCentered(emojiFor(tile.icon), tile.x, kHolderRowCy, kHolderEmoji);
    }
    for (const ClearTile& clear : clearing_) {
        const float scale = (clear.age <= clear.wait)
                                ? 1.0F
                                : clearScale(clampUnit((clear.age - clear.wait) / kClearDur));
        if (scale > 0.02F) {
            canvas.emojiCentered(emojiFor(clear.icon), clear.x, clear.y, kHolderEmoji * scale);
        }
    }
}

// Fruit in flight from the board to the holder, drawn on top of everything so it
// reads as lifted (the original raises its z while flying).
void TapMatchScene::drawFlying(Canvas& canvas) const {
    for (const FlyTile& fly : flying_) {
        const float p = clampUnit(fly.t / fly.dur);
        float tx = fly.targetX;
        float ty = fly.targetY;
        if (!fly.triple) {
            const int idx = trayIndexOf(fly.uid);
            tx = (idx >= 0) ? slotCenterX(idx) : fly.x0; // retarget as the tray reflows
            ty = kHolderRowCy;
        }
        const float e = easeOutQuad(p);
        const float size = lerp(fly.size0, kHolderEmoji, easeOutBack(p)); // slight landing pop
        canvas.emojiCentered(emojiFor(fly.icon), lerp(fly.x0, tx, e), lerp(fly.y0, ty, e), size);
    }
}

void TapMatchScene::drawSparks(Canvas& canvas) const {
    for (const Spark& spark : sparks_) {
        const float radius = lerp(7.0F, 1.0F, clampUnit(spark.age / spark.life));
        canvas.fillCircle(spark.x, spark.y, radius, theme().tmTileLight);
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
    drawTray(canvas);
    drawFlying(canvas);
    drawSparks(canvas);
    if (phase_ == Phase::GameOver) {
        drawOverlay(canvas);
    }
}

} // namespace og
