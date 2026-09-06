#include "games/mahjong/MahjongScene.hpp"

#include "core/Canvas.hpp"
#include "core/Color.hpp"
#include "core/Easing.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"
#include "games/mahjong/MahjongBoard.hpp"
#include "games/mahjong/MahjongLayouts.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <numeric>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace og {
namespace {

// ---- Chrome -------------------------------------------------------------------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kButtonRadius = 56.0F;
constexpr float kDiffLabelCy = 70.0F;
constexpr float kLevelLabelCy = 132.0F;
constexpr float kStatusCy = 196.0F;
constexpr float kToolRowCy = 1352.0F;
constexpr float kUndoCx = 150.0F;
constexpr float kHintCx = layout::kWidthF / 2.0F;
constexpr float kShuffleCx = layout::kWidthF - 150.0F;

// ---- Board play area ----------------------------------------------------------
constexpr float kAreaX = 20.0F;
constexpr float kAreaTop = 236.0F;
constexpr float kAreaBottom = 1270.0F;
constexpr float kAreaW = layout::kWidthF - (2.0F * kAreaX);
constexpr float kAreaH = kAreaBottom - kAreaTop;
constexpr float kMaxTileW = 90.0F;
constexpr float kTileAspect = 4.0F / 3.0F; // height / width
constexpr float kLiftFrac = 0.12F;         // layer lift as a fraction of tile width
constexpr float kGapPx = 1.5F;             // hairline between neighbouring faces
constexpr float kRadiusFrac = 0.12F;

// ---- Motion -------------------------------------------------------------------
constexpr float kVanishSeconds = 0.32F;
constexpr float kShakeSeconds = 0.3F;
constexpr float kHintSeconds = 2.2F;
constexpr float kShuffleSeconds = 0.35F;
constexpr float kButtonRowY = 820.0F;

constexpr const char* kUndo = "\xE2\x86\xA9";          // ↩
constexpr const char* kHint = "\xF0\x9F\x92\xA1";      // 💡
constexpr const char* kShuffle = "\xF0\x9F\x94\x80";   // 🔀
constexpr const char* kRedDragon = "\xF0\x9F\x80\x84"; // 🀄
constexpr std::array<const char*, 4> kFlowers{
    "\xF0\x9F\x8C\xB8", "\xF0\x9F\x8C\xBC", "\xF0\x9F\x8C\xBB", "\xF0\x9F\x8C\xB7"}; // 🌸 🌼 🌻 🌷
constexpr std::array<const char*, 4> kSeasons{"\xF0\x9F\x8C\xB1", "\xE2\x98\x80",
                                              "\xF0\x9F\x8D\x82", "\xE2\x9D\x84"}; // 🌱 ☀ 🍂 ❄
constexpr std::array<const char*, 4> kWinds{"E", "S", "W", "N"};

// Dot / bamboo arrangements on a 3x3 grid, one bitmask per count 1..9: bit
// (row * 3 + col) set means a mark at that cell.
constexpr std::array<std::uint16_t, 9> kSpotMasks{
    0b000010000, // 1: centre
    0b010000010, // 2: top and bottom, middle column
    0b100010001, // 3: a diagonal
    0b101000101, // 4: corners
    0b101010101, // 5: corners and centre
    0b101101101, // 6: two full columns
    0b111101101, // 7: full top row over two columns
    0b111101111, // 8: everything but the centre
    0b111111111, // 9: all
};

// Call `mark(col, row)` for every cell of `count`'s arrangement.
template <typename Fn> void forEachSpot(int count, Fn mark) {
    const std::uint16_t mask = kSpotMasks.at(static_cast<std::size_t>(std::clamp(count, 1, 9) - 1));
    for (int cell = 0; cell < 9; ++cell) {
        if ((mask & (1U << static_cast<unsigned>(cell))) != 0) {
            mark(cell % 3, cell / 3);
        }
    }
}

// The Settings field holding the current level for a difficulty.
[[nodiscard]] int& savedLevelField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.mahjongLevelEasy;
    case Difficulty::Medium:
        return s.mahjongLevelMedium;
    case Difficulty::Hard:
    case Difficulty::VeryHard:
        return s.mahjongLevelHard;
    }
    return s.mahjongLevelEasy;
}

void setSavedLevel(Difficulty difficulty, int level) {
    savedLevelField(settings(), difficulty) = std::max(1, level);
    saveSettings(settings());
}

[[nodiscard]] Color withAlpha(Color c, float alpha) {
    c.a = static_cast<std::uint8_t>(std::lround(255.0F * ease::clampUnit(alpha)));
    return c;
}

// The face of a tile: dots, bamboo sticks, a red number, a wind letter, a
// dragon, or a flower / season emoji, centred on (cx, cy) in a w x h face.
void drawSymbol(Canvas& canvas, const MahjongBoard::Tile& tile, float cx, float cy, float w,
                float h) {
    const int g = tile.group;
    if (g >= MahjongBoard::kDots && g < MahjongBoard::kBamboo) {
        const int n = g - MahjongBoard::kDots + 1;
        float r = w * 0.09F;
        if (n == 1) {
            r = w * 0.2F;
        } else if (n <= 5) {
            r = w * 0.105F;
        }
        forEachSpot(n, [&](int col, int row) {
            const float x = cx + ((static_cast<float>(col) - 1.0F) * w * 0.27F);
            const float y = cy + ((static_cast<float>(row) - 1.0F) * h * 0.25F);
            canvas.fillCircle(x, y, r, colors::mahjongDot);
            canvas.fillCircle(x, y, r * 0.42F, n == 1 ? colors::mahjongNumber : theme().mjTileFace);
        });
        return;
    }
    if (g >= MahjongBoard::kBamboo && g < MahjongBoard::kNumbers) {
        const int n = g - MahjongBoard::kBamboo + 1;
        const float sw = w * 0.13F;
        const float sh = n == 1 ? h * 0.52F : h * 0.2F;
        forEachSpot(n, [&](int col, int row) {
            const float x = cx + ((static_cast<float>(col) - 1.0F) * w * 0.27F);
            const float y = cy + ((static_cast<float>(row) - 1.0F) * h * 0.25F);
            canvas.fillRoundedRect(x - (sw / 2.0F), y - (sh / 2.0F), sw, sh, sw * 0.45F,
                                   colors::mahjongBamboo);
            canvas.fillRect(x - (sw / 2.0F), y - (sw * 0.18F), sw, sw * 0.36F, theme().mjTileFace);
        });
        return;
    }
    if (g >= MahjongBoard::kNumbers && g < MahjongBoard::kWinds) {
        const int n = g - MahjongBoard::kNumbers + 1;
        canvas.textCentered(std::to_string(n), cx, cy - (h * 0.03F), h * 0.56F,
                            colors::mahjongNumber);
        return;
    }
    if (g >= MahjongBoard::kWinds && g < MahjongBoard::kDragons) {
        canvas.textCentered(kWinds.at(static_cast<std::size_t>(g - MahjongBoard::kWinds)), cx,
                            cy - (h * 0.03F), h * 0.5F, colors::mahjongInk);
        return;
    }
    if (g == MahjongBoard::kDragons) {
        canvas.emojiCentered(kRedDragon, cx, cy, h * 0.6F);
        return;
    }
    if (g == MahjongBoard::kDragons + 1) {
        canvas.textCentered("F", cx, cy - (h * 0.03F), h * 0.52F, colors::mahjongBamboo);
        return;
    }
    if (g == MahjongBoard::kDragons + 2) {
        // The white dragon: an empty frame.
        const float fw = w * 0.5F;
        const float fh = h * 0.5F;
        const float t = w * 0.07F;
        canvas.fillRoundedRect(cx - (fw / 2.0F), cy - (fh / 2.0F), fw, fh, t, colors::mahjongDot);
        canvas.fillRoundedRect(cx - (fw / 2.0F) + t, cy - (fh / 2.0F) + t, fw - (2.0F * t),
                               fh - (2.0F * t), t * 0.5F, theme().mjTileFace);
        return;
    }
    const auto face = static_cast<std::size_t>(std::clamp(tile.face, 0, 3));
    canvas.emojiCentered(g == MahjongBoard::kFlowers ? kFlowers.at(face) : kSeasons.at(face), cx,
                         cy, h * 0.58F);
}

// The "no moves" panel over the middle of the board.
void drawBanner(Canvas& canvas) {
    constexpr float kW = 520.0F;
    constexpr float kH = 150.0F;
    const float x = (layout::kWidthF - kW) / 2.0F;
    const float y = 700.0F;
    canvas.fillRoundedRect(x, y, kW, kH, 28.0F, withAlpha(colors::gridBlack, 0.82F));
    canvas.textCentered("NO MOVES LEFT", layout::kWidthF / 2.0F, y + 52.0F, 44.0F, colors::white);
    canvas.textCentered("Shuffle the tiles or undo", layout::kWidthF / 2.0F, y + 108.0F, 28.0F,
                        colors::textMuted);
}

} // namespace

int mahjongSavedLevel(Difficulty difficulty) {
    return savedLevelField(settings(), difficulty);
}

MahjongScene::MahjongScene(SceneManager& manager, Difficulty difficulty, int level)
    : manager_(manager), difficulty_(difficulty), level_(std::max(1, level)),
      layoutName_(mahjongLayoutFor(difficulty, level_).name),
      board_(mahjongLayoutFor(difficulty, level_).slots, mahjongLevelSeed(difficulty, level_)),
      backButton_(IconButton::Icon::Back, kBackCx, kBackCy, kButtonRadius),
      undoButton_(IconButton::Icon::Glyph, kUndoCx, kToolRowCy, kButtonRadius),
      hintButton_(IconButton::Icon::Glyph, kHintCx, kToolRowCy, kButtonRadius),
      shuffleButton_(IconButton::Icon::Glyph, kShuffleCx, kToolRowCy, kButtonRadius),
      overlay_(color(difficulty_), colors::white, kButtonRowY) {
    backButton_.setOnTap([this] { manager_.pop(); });
    undoButton_.setGlyph(kUndo, 54.0F);
    undoButton_.setOnTap([this] { onUndo(); });
    hintButton_.setGlyph(kHint, 54.0F);
    hintButton_.setOnTap([this] { onHint(); });
    shuffleButton_.setGlyph(kShuffle, 54.0F);
    shuffleButton_.setOnTap([this] { onShuffle(); });
    overlay_.setOnHome([this] { manager_.popToRoot(); });
    overlay_.setActionLabel("NEXT");
    overlay_.setOnAction([this] {
        manager_.replace(std::make_unique<MahjongScene>(manager_, difficulty_, level_ + 1));
    });

    drawOrder_.resize(board_.tiles().size());
    std::iota(drawOrder_.begin(), drawOrder_.end(), 0);
    // Back to front: lower layers first, then top to bottom, left to right.
    std::ranges::sort(drawOrder_, [this](int a, int b) {
        const MahjongBoard::Slot& sa = board_.tiles().at(static_cast<std::size_t>(a)).slot;
        const MahjongBoard::Slot& sb = board_.tiles().at(static_cast<std::size_t>(b)).slot;
        return std::tie(sa.z, sa.y, sa.x) < std::tie(sb.z, sb.y, sb.x);
    });
    layoutBoard();
}

void MahjongScene::layoutBoard() {
    const auto cols = static_cast<float>(board_.width()) / 2.0F;
    const auto rows = static_cast<float>(board_.height()) / 2.0F;
    const auto liftLayers = static_cast<float>(std::max(0, board_.layers() - 1)) * kLiftFrac;
    // The stack leans up-left by lift per layer, which adds to both extents.
    const float wByWidth = kAreaW / (cols + liftLayers);
    const float wByHeight = kAreaH / ((rows * kTileAspect) + liftLayers);
    tileW_ = std::min({wByWidth, wByHeight, kMaxTileW});
    tileH_ = tileW_ * kTileAspect;
    lift_ = tileW_ * kLiftFrac;
    const float totalLift = lift_ * static_cast<float>(std::max(0, board_.layers() - 1));
    const float boardW = (cols * tileW_) + totalLift;
    const float boardH = (rows * tileH_) + totalLift;
    originX_ = kAreaX + ((kAreaW - boardW) / 2.0F) + totalLift;
    originY_ = kAreaTop + ((kAreaH - boardH) / 2.0F) + totalLift;
}

MahjongScene::Rect MahjongScene::faceRect(const MahjongBoard::Tile& tile) const {
    const auto z = static_cast<float>(tile.slot.z);
    return {.x = originX_ + (static_cast<float>(tile.slot.x) / 2.0F * tileW_) - (z * lift_),
            .y = originY_ + (static_cast<float>(tile.slot.y) / 2.0F * tileH_) - (z * lift_),
            .w = tileW_,
            .h = tileH_};
}

int MahjongScene::tileAt(float px, float py) const {
    // Front to back, so a raised tile wins over the one it half covers.
    for (const int id : std::views::reverse(drawOrder_)) {
        const MahjongBoard::Tile& tile = board_.tiles().at(static_cast<std::size_t>(id));
        if (tile.removed) {
            continue;
        }
        const Rect r = faceRect(tile);
        if (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h) {
            return tile.id;
        }
    }
    return -1;
}

void MahjongScene::tapAt(float px, float py) {
    const int id = tileAt(px, py);
    if (id < 0) {
        return;
    }
    const int partner = board_.selected(); // the pair's other half, if this matches
    switch (board_.tap(id)) {
    case MahjongBoard::Tap::Matched:
        vanishes_.push_back({.id = id, .t = 0.0F});
        vanishes_.push_back({.id = partner, .t = 0.0F});
        hintT_ = 0.0F;
        noMoves_ = !board_.isWon() && !board_.hasMoves();
        break;
    case MahjongBoard::Tap::Blocked:
        shakeId_ = id;
        shakeT_ = 0.0F;
        break;
    case MahjongBoard::Tap::Selected:
    case MahjongBoard::Tap::Deselected:
    case MahjongBoard::Tap::Ignored:
        break;
    }
}

void MahjongScene::onHint() {
    const auto [a, b] = board_.hint();
    if (a < 0) {
        noMoves_ = !board_.isWon();
        return;
    }
    hintA_ = a;
    hintB_ = b;
    hintT_ = kHintSeconds;
}

void MahjongScene::onShuffle() {
    if (board_.shuffle()) {
        shuffleT_ = 0.0F;
        hintT_ = 0.0F;
        noMoves_ = false;
        vanishes_.clear();
    }
}

void MahjongScene::onUndo() {
    if (board_.undo()) {
        hintT_ = 0.0F;
        noMoves_ = false;
        vanishes_.clear();
    }
}

void MahjongScene::handleInput(const PointerEvent& event) {
    if (backButton_.handleInput(event)) {
        return;
    }
    if (phase_ == Phase::Won) {
        overlay_.handleInput(event);
        return;
    }
    if (undoButton_.handleInput(event) || hintButton_.handleInput(event) ||
        shuffleButton_.handleInput(event)) {
        return;
    }
    if (event.phase == PointerEvent::Phase::Down) {
        tapAt(event.x, event.y);
    }
}

void MahjongScene::finishIfOver() {
    if (phase_ != Phase::Playing || !vanishes_.empty() || !board_.isWon()) {
        return;
    }
    phase_ = Phase::Won;
    if (level_ + 1 > mahjongSavedLevel(difficulty_)) {
        setSavedLevel(difficulty_, level_ + 1);
    }
}

void MahjongScene::update(float dtSeconds) {
    for (Vanish& v : vanishes_) {
        v.t += dtSeconds;
    }
    std::erase_if(vanishes_, [](const Vanish& v) { return v.t >= kVanishSeconds; });
    shakeT_ = std::min(kShakeSeconds, shakeT_ + dtSeconds);
    shuffleT_ = std::min(kShuffleSeconds, shuffleT_ + dtSeconds);
    hintT_ = std::max(0.0F, hintT_ - dtSeconds);
    finishIfOver();
}

bool MahjongScene::isAnimating() const {
    return !vanishes_.empty() || shakeT_ < kShakeSeconds || shuffleT_ < kShuffleSeconds ||
           hintT_ > 0.0F;
}

// ---- Drawing ------------------------------------------------------------------

void MahjongScene::drawTopBar(Canvas& canvas) const {
    backButton_.render(canvas);
    canvas.textCentered(label(difficulty_), layout::kWidthF / 2.0F, kDiffLabelCy, 30.0F,
                        color(difficulty_));
    canvas.textCentered("Level " + std::to_string(level_), layout::kWidthF / 2.0F, kLevelLabelCy,
                        56.0F, theme().primaryText);
    canvas.textCentered(layoutName_ + "  \xC2\xB7  " + std::to_string(board_.remaining()) + " left",
                        layout::kWidthF / 2.0F, kStatusCy, 28.0F, theme().mjStatusText);
    undoButton_.render(canvas);
    hintButton_.render(canvas);
    shuffleButton_.render(canvas);
}

void MahjongScene::drawTile(Canvas& canvas, const MahjongBoard::Tile& tile, Rect r, float scale,
                            Color face) const {
    const float cx = r.x + (r.w / 2.0F);
    const float cy = r.y + (r.h / 2.0F);
    const float w = (r.w - (2.0F * kGapPx)) * scale;
    const float h = (r.h - (2.0F * kGapPx)) * scale;
    const float radius = tileW_ * kRadiusFrac * scale;
    canvas.fillRoundedRect(cx - (w / 2.0F), cy - (h / 2.0F), w, h, radius, face);
    drawSymbol(canvas, tile, cx, cy, w, h);
}

void MahjongScene::drawFace(Canvas& canvas, const MahjongBoard::Tile& tile, float hintPulse,
                            float bounce) const {
    if (tile.removed) {
        return;
    }
    Rect r = faceRect(tile);
    if (tile.id == shakeId_ && shakeT_ < kShakeSeconds) {
        r.x +=
            tileW_ * 0.08F * std::sin(shakeT_ / kShakeSeconds * 3.0F * std::numbers::pi_v<float>);
    }
    if (hintT_ > 0.0F && (tile.id == hintA_ || tile.id == hintB_)) {
        const float grow = 3.0F + (3.0F * hintPulse);
        canvas.fillRoundedRect(r.x - grow, r.y - grow, r.w + (2.0F * grow), r.h + (2.0F * grow),
                               (tileW_ * kRadiusFrac) + grow, colors::accent);
    }
    Color face = theme().mjTileFace;
    if (tile.id == board_.selected()) {
        face = colors::mahjongSelected;
    } else if (!board_.isFree(tile.id)) {
        face = theme().mjTileDim;
    }
    drawTile(canvas, tile, r, bounce, face);
}

void MahjongScene::drawTiles(Canvas& canvas) const {
    const float hintPulse =
        0.5F + (0.5F * std::sin(hintT_ * 2.0F * std::numbers::pi_v<float> * 1.6F));
    float bounce = 1.0F;
    if (shuffleT_ < kShuffleSeconds) {
        bounce =
            1.0F - (0.12F * std::sin(std::numbers::pi_v<float> * (shuffleT_ / kShuffleSeconds)));
    }
    const auto tileOf = [this](std::size_t i) -> const MahjongBoard::Tile& {
        return board_.tiles().at(static_cast<std::size_t>(drawOrder_.at(i)));
    };
    // Per layer: every tile's side first, then every face, so faces always
    // cover the sides of their same-layer neighbours whatever the offsets.
    std::size_t begin = 0;
    while (begin < drawOrder_.size()) {
        const int z = tileOf(begin).slot.z;
        std::size_t end = begin;
        while (end < drawOrder_.size() && tileOf(end).slot.z == z) {
            ++end;
        }
        for (std::size_t i = begin; i < end; ++i) {
            if (tileOf(i).removed) {
                continue;
            }
            const Rect r = faceRect(tileOf(i));
            canvas.fillRoundedRect(r.x + lift_ + kGapPx, r.y + lift_ + kGapPx,
                                   r.w - (2.0F * kGapPx), r.h - (2.0F * kGapPx),
                                   tileW_ * kRadiusFrac, theme().mjTileSide);
        }
        for (std::size_t i = begin; i < end; ++i) {
            drawFace(canvas, tileOf(i), hintPulse, bounce);
        }
        begin = end;
    }
}

void MahjongScene::drawVanishes(Canvas& canvas) const {
    for (const Vanish& v : vanishes_) {
        const MahjongBoard::Tile& tile = board_.tiles().at(static_cast<std::size_t>(v.id));
        const float t = ease::clampUnit(v.t / kVanishSeconds);
        // Swell a touch, then shrink away.
        const float scale = t < 0.3F ? 1.0F + (0.25F * (t / 0.3F))
                                     : 1.25F * (1.0F - ease::easeInCubic((t - 0.3F) / 0.7F));
        if (scale <= 0.02F) {
            continue;
        }
        drawTile(canvas, tile, faceRect(tile), scale, colors::mahjongSelected);
    }
}

void MahjongScene::drawOverlay(Canvas& canvas) const {
    overlay_.render(canvas, "CLEARED!", 600.0F, 96.0F);
    const std::string detail = board_.shuffles() == 0
                                   ? "No shuffles needed"
                                   : "Shuffles: " + std::to_string(board_.shuffles());
    canvas.textCentered(detail, layout::kWidthF / 2.0F, 690.0F, 32.0F, colors::white);
}

void MahjongScene::render(Canvas& canvas) {
    canvas.clear(theme().mjTable);
    drawTiles(canvas);
    drawVanishes(canvas);
    drawTopBar(canvas);
    if (noMoves_ && phase_ == Phase::Playing) {
        drawBanner(canvas);
    }
    if (phase_ == Phase::Won) {
        drawOverlay(canvas);
    }
}

} // namespace og
