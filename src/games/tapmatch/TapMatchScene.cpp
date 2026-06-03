#include "games/tapmatch/TapMatchScene.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"
#include "games/tapmatch/TapMatchLevels.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace og {
namespace {

// ---- Back button (circular, top-left) — matches the other scenes -----------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;

// ---- Board play area: authored boards are fitted/centred into this rect ------
// (Tile size and origin are computed per board in layoutBoard, since the
// original levels vary in extent; only the bounding area below is fixed.)
constexpr float kBoardAreaX = 40.0F;
constexpr float kBoardAreaTop = 196.0F;
constexpr float kBoardAreaBottom = 1126.0F; // just above the holder bar
constexpr float kBoardAreaW = layout::kWidthF - (2.0F * kBoardAreaX);
constexpr float kBoardAreaH = kBoardAreaBottom - kBoardAreaTop;
constexpr float kMaxCellPx = 40.0F; // cap so small boards don't get huge tiles
// Tile visuals as fractions of the (variable) tile footprint (kTileSpan cells).
constexpr float kTileInsetFrac = 0.066F; // grey margin around the card
constexpr float kTileRadiusFrac = 0.21F; // card corner radius
constexpr float kTileEdgeFrac = 0.052F;  // border separating stacked tiles
constexpr float kEmojiFrac = 0.605F;     // fruit size on the card

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
constexpr float kClearHold = 0.10F; // show the completed triple before it pops
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

// Difficulty -> complexity tier index (Easy=0, Medium=1, Hard=2).
[[nodiscard]] int difficultyTier(Difficulty difficulty) {
    return static_cast<int>(difficulty);
}

// The Settings field holding the current level for a difficulty.
[[nodiscard]] int& savedLevelField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.tapmatchLevelEasy;
    case Difficulty::Medium:
        return s.tapmatchLevelMedium;
    case Difficulty::Hard:
        return s.tapmatchLevelHard;
    case Difficulty::VeryHard:
        break; // Tap Match offers only three difficulties
    }
    return s.tapmatchLevelMedium;
}

void setSavedLevel(Difficulty difficulty, int level) {
    savedLevelField(settings(), difficulty) = std::max(1, level);
    saveSettings(settings());
}

// Build the board for (difficulty, 1-based level) from that difficulty's pool of
// original boards. Falls back to a small procedural board if the asset is missing.
[[nodiscard]] TapMatchBoard boardFor(Difficulty difficulty, int level) {
    const LevelLayout* layout = tapMatchTierLevel(difficultyTier(difficulty), level);
    if (layout != nullptr) {
        return {*layout, std::random_device{}()};
    }
    return {TapMatchBoard::GenParams{}, std::random_device{}()};
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

int tapMatchSavedLevel(Difficulty difficulty) {
    return savedLevelField(settings(), difficulty);
}

TapMatchScene::TapMatchScene(SceneManager& manager, Difficulty difficulty, int level)
    : manager_(manager), difficulty_(difficulty), level_(level),
      board_(boardFor(difficulty, level)),
      homeButton_("\xF0\x9F\x8F\xA0", kRowX, kButtonRowY, kHomeSize, kHomeSize), // 🏠
      playAgainButton_("PLAY AGAIN", kRowX + kHomeSize + kButtonGap, kButtonRowY, kPlayAgainW,
                       kHomeSize) {
    homeButton_.setColors(colors::white, colors::panelBrown);
    homeButton_.setOnTap([this] { manager_.popToRoot(); });
    // The action button's label/handler are set per result in enterGameOver().
    playAgainButton_.setColors(colors::menuPink, colors::white);
    layoutBoard();
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
    const float cell = boardCellPx_;
    const float tilePx = cell * static_cast<float>(TapMatchBoard::kTileSpan);
    int best = -1;
    int bestLayer = -1;
    for (const auto& tile : board_.tiles()) {
        if (tile.removed) {
            continue;
        }
        const float x = boardOriginX_ + (static_cast<float>(tile.x) * cell);
        const float y = boardOriginY_ + (static_cast<float>(tile.y) * cell);
        if (px >= x && px < x + tilePx && py >= y && py < y + tilePx && tile.layer > bestLayer) {
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
    board_ = boardFor(difficulty_, level_);
    phase_ = Phase::Playing;
    layoutBoard();
    resetFx();
}

void TapMatchScene::enterGameOver() {
    phase_ = Phase::GameOver;
    // Set the action button by result: advance, replay the final level, or retry.
    const bool won = board_.result() == TapMatchBoard::Result::Won;
    const int lastLevel = tapMatchTierSize(difficultyTier(difficulty_));
    if (won && level_ < lastLevel) {
        playAgainButton_.setLabel("NEXT");
        playAgainButton_.setOnTap([this] {
            manager_.replace(std::make_unique<TapMatchScene>(manager_, difficulty_, level_ + 1));
        });
    } else {
        playAgainButton_.setLabel(won ? "REPLAY" : "RETRY");
        playAgainButton_.setOnTap([this] { beginRound(); });
    }
}

void TapMatchScene::layoutBoard() {
    const auto gw = static_cast<float>(std::max(1, board_.gridWidth()));
    const auto gh = static_cast<float>(std::max(1, board_.gridHeight()));
    boardCellPx_ = std::min({kBoardAreaW / gw, kBoardAreaH / gh, kMaxCellPx});
    boardOriginX_ = kBoardAreaX + ((kBoardAreaW - (gw * boardCellPx_)) / 2.0F);
    boardOriginY_ = kBoardAreaTop + ((kBoardAreaH - (gh * boardCellPx_)) / 2.0F);
}

void TapMatchScene::resetFx() {
    tray_.clear();
    flying_.clear();
    sparks_.clear();
    introClock_ = 0.0F;
    nextUid_ = 1;
    wonPending_ = false;
    losePending_ = false;
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

// Tap a board tile and fly its fruit to the holder. The board model updates
// instantly; tray_ animates that change. board_.holder() is queried *before* the
// tap (it's the source of truth for grouping). The third of a match flies in and
// settles into its slot like any other fruit — so the holder shows the completed
// triple — and only then do the three pop out (see maybeStartClear).
void TapMatchScene::startTap(int id) {
    const TapMatchBoard::Tile& tile = board_.tiles().at(static_cast<std::size_t>(id));
    const int icon = tile.icon;
    const float cell = boardCellPx_;
    const float tilePx = cell * static_cast<float>(TapMatchBoard::kTileSpan);
    const float cx = boardOriginX_ + (static_cast<float>(tile.x) * cell) + (tilePx / 2.0F);
    const float cy = boardOriginY_ + (static_cast<float>(tile.y) * cell) + (tilePx / 2.0F);

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

    // If the holder's visuals are full of still-popping fruit (a fast tap during
    // another match's pop), snap those away so the new fruit has a free slot.
    if (tray_.size() >= static_cast<std::size_t>(kSlots)) {
        std::erase_if(flying_, [this](const FlyTile& f) {
            const int idx = trayIndexOf(f.uid);
            return idx >= 0 && tray_.at(static_cast<std::size_t>(idx)).pending;
        });
        std::erase_if(tray_, [](const TrayTile& t) { return t.pending; });
    }

    // Destination slot = group with this icon's active (non-pending) fruit, or end.
    int slot = static_cast<int>(tray_.size());
    for (std::size_t i = 0; i < tray_.size(); ++i) {
        const TrayTile& t = tray_.at(i);
        if (t.icon == icon && !t.pending) {
            slot = static_cast<int>(i);
            break;
        }
    }
    const int uid = nextUid_++;
    tray_.insert(tray_.begin() + slot, TrayTile{.uid = uid,
                                                .icon = icon,
                                                .x = slotCenterX(slot),
                                                .arriving = true,
                                                .pending = willTriple,
                                                .clear = -1.0F});
    flying_.push_back(FlyTile{.uid = uid,
                              .icon = icon,
                              .x0 = cx,
                              .y0 = cy,
                              .size0 = tilePx * kEmojiFrac,
                              .t = 0.0F,
                              .dur = flyDuration(cx, cy, slotCenterX(slot), kHolderRowCy)});

    if (willTriple) {
        // The two existing fruit of this icon leave with the match too.
        for (TrayTile& t : tray_) {
            if (t.icon == icon && !t.pending) {
                t.pending = true;
            }
        }
    }

    if (board_.result() == TapMatchBoard::Result::Won) {
        wonPending_ = true; // defer the overlay until the last match finishes
        // Advance (and persist) this difficulty's progress when a new level falls.
        const int lastLevel = tapMatchTierSize(difficultyTier(difficulty_));
        if (level_ < lastLevel && level_ + 1 > tapMatchSavedLevel(difficulty_)) {
            setSavedLevel(difficulty_, level_ + 1);
        }
    } else if (board_.result() == TapMatchBoard::Result::Lost) {
        losePending_ = true; // let the fruit land in the last slot, then game over
    }
}

void TapMatchScene::advanceFx(float dtSeconds) {
    if (dtSeconds <= 0.0F) {
        return;
    }
    introClock_ += dtSeconds;

    // Flights: advance; on landing the fruit settles into its slot, and if that
    // completes a match (all three landed) the group starts popping.
    for (auto it = flying_.begin(); it != flying_.end();) {
        it->t += dtSeconds;
        if (it->t < it->dur) {
            ++it;
            continue;
        }
        const int idx = trayIndexOf(it->uid);
        if (idx >= 0) {
            tray_.at(static_cast<std::size_t>(idx)).arriving = false;
            maybeStartClear(it->icon);
        }
        it = flying_.erase(it);
    }

    // Holder reflow: slide each fruit toward its slot. Popping fruit hold their
    // slot until removed, so the tray only collapses once a match has finished.
    const float slide = 1.0F - std::exp(-kSlideRate * dtSeconds);
    for (std::size_t i = 0; i < tray_.size(); ++i) {
        TrayTile& tile = tray_.at(i);
        tile.x += (slotCenterX(static_cast<int>(i)) - tile.x) * slide;
    }

    // Pops: advance the clear timer and drop fully-popped fruit (tray collapses).
    for (auto it = tray_.begin(); it != tray_.end();) {
        if (it->clear >= 0.0F) {
            it->clear += dtSeconds;
            if (it->clear >= kClearHold + kClearDur) {
                it = tray_.erase(it);
                continue;
            }
        }
        ++it;
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

    // Show the result overlay once the final fruit has landed and any match has
    // finished popping (a win clears the board; a loss fills the last slot).
    if ((wonPending_ || losePending_) && flying_.empty() && !anyPending()) {
        wonPending_ = false;
        losePending_ = false;
        enterGameOver();
    }
}

// Start a match's pop once all three of `icon`'s pending fruit have landed: the
// holder has shown the completed triple, now burst it.
void TapMatchScene::maybeStartClear(int icon) {
    float sumX = 0.0F;
    int count = 0;
    for (const TrayTile& tile : tray_) {
        if (tile.icon == icon && tile.pending && tile.clear < 0.0F) {
            if (tile.arriving) {
                return; // a member of this match is still in flight
            }
            sumX += tile.x;
            ++count;
        }
    }
    if (count == 0) {
        return;
    }
    for (TrayTile& tile : tray_) {
        if (tile.icon == icon && tile.pending && tile.clear < 0.0F) {
            tile.clear = 0.0F;
        }
    }
    spawnSparks(sumX / static_cast<float>(count), kHolderRowCy);
}

bool TapMatchScene::anyPending() const {
    return std::ranges::any_of(tray_, [](const TrayTile& t) { return t.pending; });
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
    canvas.textCentered("Level " + std::to_string(level_), layout::kWidthF / 2.0F, 92.0F, 52.0F,
                        theme().primaryText);
    canvas.textCentered(statusText(), layout::kWidthF / 2.0F, 150.0F, 28.0F, theme().tmStatusText);
}

void TapMatchScene::drawBoard(Canvas& canvas) const {
    const bool intro = introActive();
    const float cell = boardCellPx_;
    const float tilePx = cell * static_cast<float>(TapMatchBoard::kTileSpan);
    const float inset = tilePx * kTileInsetFrac;
    const float radius = tilePx * kTileRadiusFrac;
    const float edge = tilePx * kTileEdgeFrac;
    const float emojiPx = tilePx * kEmojiFrac;
    // tiles() is ordered by ascending layer, so iterating in order paints higher
    // (covering) tiles on top of the ones they sit over.
    for (const auto& tile : board_.tiles()) {
        if (tile.removed) {
            continue;
        }
        float cx = boardOriginX_ + (static_cast<float>(tile.x) * cell) + (tilePx / 2.0F);
        float cy = boardOriginY_ + (static_cast<float>(tile.y) * cell) + (tilePx / 2.0F);
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
        const float cardW = (tilePx - (2.0F * inset)) * scale;
        const float border = 2.0F * edge; // total extra width (edge on each side)
        // A grey border (slightly larger) separates overlapping tiles, then the
        // card itself (white & raised when free, grey when covered).
        canvas.fillRoundedRect(cx - ((cardW + border) / 2.0F), cy - ((cardW + border) / 2.0F),
                               cardW + border, cardW + border, (radius + edge) * scale,
                               theme().tmTileEdge);
        canvas.fillRoundedRect(cx - (cardW / 2.0F), cy - (cardW / 2.0F), cardW, cardW,
                               radius * scale,
                               accessible ? theme().tmTileLight : theme().tmTileDim);
        canvas.emojiCentered(emojiFor(tile.icon), cx, cy, emojiPx * scale);
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

// The held fruit (animated x as the tray reflows), including a completing triple
// held at full size for a beat before it pops and shrinks out. Drawn above the
// holder slots but below the in-flight fruit.
void TapMatchScene::drawTray(Canvas& canvas) const {
    for (const TrayTile& tile : tray_) {
        if (tile.arriving) {
            continue; // still drawn by its flight until it lands
        }
        float scale = 1.0F;
        if (tile.clear >= kClearHold) {
            scale = clearScale(clampUnit((tile.clear - kClearHold) / kClearDur));
        }
        if (scale > 0.02F) {
            canvas.emojiCentered(emojiFor(tile.icon), tile.x, kHolderRowCy, kHolderEmoji * scale);
        }
    }
}

// Fruit in flight from the board to the holder, drawn on top of everything so it
// reads as lifted (the original raises its z while flying).
void TapMatchScene::drawFlying(Canvas& canvas) const {
    for (const FlyTile& fly : flying_) {
        const float p = clampUnit(fly.t / fly.dur);
        const int idx = trayIndexOf(fly.uid);
        const float tx = (idx >= 0) ? slotCenterX(idx) : fly.x0; // retarget as the tray reflows
        const float e = easeOutQuad(p);
        const float size = lerp(fly.size0, kHolderEmoji, easeOutBack(p)); // slight landing pop
        canvas.emojiCentered(emojiFor(fly.icon), lerp(fly.x0, tx, e), lerp(fly.y0, kHolderRowCy, e),
                             size);
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
