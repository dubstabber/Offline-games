#include "games/memory/MemoryScene.hpp"

#include "core/Canvas.hpp"
#include "core/Easing.hpp"
#include "core/GridLayout.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Theme.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <random>
#include <string>

namespace og {
namespace {

using Player = MemoryBoard::Player;

// ---- Top bar ----------------------------------------------------------------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;
constexpr float kRestartCx = layout::kWidthF - 92.0F;
constexpr const char* kRestartGlyph = "\xF0\x9F\x94\x84"; // 🔄

// ---- Scoreboard panel (between the two buttons, like Tic-Tac-Toe's) ----------
constexpr float kPanelW = 400.0F;
constexpr float kPanelH = 92.0F;
constexpr float kPanelX = (layout::kWidthF - kPanelW) / 2.0F;
constexpr float kPanelY = 64.0F;
constexpr float kYouCx = kPanelX + 84.0F;
constexpr float kBotCx = kPanelX + kPanelW - 84.0F;
constexpr float kBannerCy = 200.0F;

// ---- Table area the cards are fitted into --------------------------------------
constexpr float kAreaX = 30.0F;
constexpr float kAreaY = 244.0F;
constexpr float kAreaW = layout::kWidthF - (2.0F * kAreaX);
constexpr float kAreaH = 1060.0F;
constexpr float kMaxCell = 168.0F;
constexpr float kCardInset = 7.0F; // gap between neighbouring cards, each side

// ---- Timing -------------------------------------------------------------------
constexpr float kDealStagger = 0.04F;   // between consecutive cards flying in
constexpr float kLookSeconds = 1.0F;    // a mismatched pair stays up this long
constexpr float kMatchHold = 0.45F;     // a matched pair is admired this long
constexpr float kBotFirstDelay = 0.7F;  // bot "thinks" before its first flip
constexpr float kBotSecondDelay = 0.6F; // and between its two flips
constexpr float kTableFadeSeconds = 0.5F;

// ---- Result overlay -----------------------------------------------------------
constexpr float kButtonRowY = 760.0F;

// The sixteen pictures the original deals from, as color emoji. Every string is
// emoji-only, so Canvas draws it with the emoji font.
constexpr std::array<const char*, MemoryBoard::kPictureCount> kPictures{
    "\xF0\x9F\x90\xB6", // 🐶 U+1F436
    "\xF0\x9F\x90\xB1", // 🐱 U+1F431
    "\xF0\x9F\xA6\x8A", // 🦊 U+1F98A
    "\xF0\x9F\x90\xBC", // 🐼 U+1F43C
    "\xF0\x9F\x90\xB8", // 🐸 U+1F438
    "\xF0\x9F\x90\xB5", // 🐵 U+1F435
    "\xF0\x9F\xA6\x81", // 🦁 U+1F981
    "\xF0\x9F\x90\x99", // 🐙 U+1F419
    "\xF0\x9F\x8D\x8E", // 🍎 U+1F34E
    "\xF0\x9F\x8D\x8C", // 🍌 U+1F34C
    "\xF0\x9F\x8D\x87", // 🍇 U+1F347
    "\xF0\x9F\x8D\x93", // 🍓 U+1F353
    "\xF0\x9F\x8C\xBB", // 🌻 U+1F33B
    "\xF0\x9F\x8C\x88", // 🌈 U+1F308
    "\xF0\x9F\x9A\x97", // 🚗 U+1F697
    "\xF0\x9F\x8E\x88", // 🎈 U+1F388
};

[[nodiscard]] int difficultyToIndex(Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return 0;
    case Difficulty::Hard:
    case Difficulty::VeryHard:
        return 2;
    case Difficulty::Medium:
        break;
    }
    return 1;
}

[[nodiscard]] Color mix(Color a, Color b, float t) {
    const auto channel = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(
            std::lround(ease::lerp(static_cast<float>(x), static_cast<float>(y), t)));
    };
    return rgb(channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b));
}

[[nodiscard]] float scoreCx(Player player) {
    return player == Player::You ? kYouCx : kBotCx;
}

[[nodiscard]] std::uint32_t randomSeed() {
    return std::random_device{}();
}

} // namespace

MemoryScene::MemoryScene(SceneManager& manager, Difficulty difficulty)
    : manager_(manager), difficulty_(difficulty),
      board_(memoryLayout(difficultyToIndex(difficulty)).pairs, randomSeed()),
      bot_(memoryBotProfile(difficulty), randomSeed()),
      backButton_(IconButton::Icon::Back, kBackCx, kBackCy, kBackRadius),
      restartButton_(IconButton::Icon::Glyph, kRestartCx, kBackCy, kBackRadius),
      overlay_(colors::youRed, colors::white, kButtonRowY) {
    backButton_.setOnTap([this] { manager_.pop(); });
    restartButton_.setGlyph(kRestartGlyph, 50.0F);
    restartButton_.setOnTap([this] { beginRound(randomSeed()); });
    overlay_.setOnHome([this] { manager_.popToRoot(); });
    overlay_.setActionLabel("PLAY AGAIN");
    overlay_.setOnAction([this] { beginRound(randomSeed()); });
    layoutCards();
    beginRound(randomSeed());
}

bool MemoryScene::isAnimating() const {
    if (phase_ != Phase::PlayerTurn && phase_ != Phase::GameOver) {
        return true;
    }
    const float target = board_.turn() == Player::Bot ? 1.0F : 0.0F;
    if (std::abs(tableMix_ - target) > 0.001F) {
        return true;
    }
    return std::ranges::any_of(views_, [](const CardView& v) {
        return v.flipT < kFlipSeconds || (v.leaveT >= 0.0F && v.leaveT < kLeaveSeconds);
    });
}

// ---- Round setup --------------------------------------------------------------

void MemoryScene::layoutCards() {
    const MemoryLayout layout = memoryLayout(difficultyToIndex(difficulty_));
    const int rows = static_cast<int>(layout.rowCounts.size());
    const int cols = *std::ranges::max_element(layout.rowCounts);
    const grid::Fit fit = grid::fitCentered(kAreaX, kAreaY, kAreaW, kAreaH, cols, rows, kMaxCell);
    cardPx_ = fit.cellPx;

    views_.assign(static_cast<std::size_t>(board_.cardCount()), CardView{});
    std::size_t index = 0;
    int row = 0;
    for (const int count : layout.rowCounts) {
        // Shorter rows are centered, giving the original's staggered layouts.
        const float rowX = fit.originX + (static_cast<float>(cols - count) * cardPx_ / 2.0F);
        for (int col = 0; col < count && index < views_.size(); ++col, ++index) {
            CardView& v = views_.at(index);
            v.cx = rowX + ((static_cast<float>(col) + 0.5F) * cardPx_);
            v.cy = fit.originY + ((static_cast<float>(row) + 0.5F) * cardPx_);
        }
        ++row;
    }
}

void MemoryScene::beginRound(std::uint32_t seed) {
    board_.reset(seed);
    bot_.reset();
    for (std::size_t i = 0; i < views_.size(); ++i) {
        CardView& v = views_.at(i);
        v.flipT = kFlipSeconds;
        v.showFace = false;
        v.dealDelay = static_cast<float>(i) * kDealStagger;
        v.dealT = 0.0F;
        v.leaveT = -1.0F;
    }
    tableMix_ = 0.0F;
    timer_ = 0.0F;
    phase_ = Phase::Dealing;
}

// ---- Input --------------------------------------------------------------------

int MemoryScene::cardAt(float x, float y) const {
    const float half = cardPx_ / 2.0F;
    for (std::size_t i = 0; i < views_.size(); ++i) {
        const CardView& v = views_.at(i);
        if (x >= v.cx - half && x < v.cx + half && y >= v.cy - half && y < v.cy + half) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void MemoryScene::handleInput(const PointerEvent& event) {
    if (backButton_.handleInput(event)) {
        return;
    }
    if (phase_ == Phase::GameOver) {
        overlay_.handleInput(event);
        return;
    }
    if (restartButton_.handleInput(event)) {
        return;
    }
    if (phase_ != Phase::PlayerTurn || event.phase != PointerEvent::Phase::Down) {
        return;
    }
    const int index = cardAt(event.x, event.y);
    if (index < 0 || !board_.canFlip(index)) {
        return;
    }
    reveal(index);
    if (board_.awaitingResolve()) {
        afterSecondFlip();
    }
}

// ---- Turn flow ----------------------------------------------------------------

void MemoryScene::reveal(int index) {
    board_.flip(index);
    bot_.observe(board_, index);
    CardView& v = views_.at(static_cast<std::size_t>(index));
    v.showFace = true;
    v.flipT = 0.0F;
}

void MemoryScene::afterSecondFlip() {
    const std::optional<int> a = board_.firstFlipped();
    const std::optional<int> b = board_.secondFlipped();
    if (!a || !b) {
        return;
    }
    const bool match = board_.card(*a).picture == board_.card(*b).picture;
    // Let the second card finish turning before the look/hold starts.
    timer_ = kFlipSeconds + (match ? kMatchHold : kLookSeconds);
    phase_ = Phase::Resolving;
}

void MemoryScene::botFlip() {
    const std::optional<int> first = board_.firstFlipped();
    if (!first) {
        reveal(bot_.chooseFirst(board_));
        timer_ = kBotSecondDelay;
        return;
    }
    reveal(bot_.chooseSecond(board_, *first));
    afterSecondFlip();
}

void MemoryScene::settle() {
    const std::optional<int> a = board_.firstFlipped();
    const std::optional<int> b = board_.secondFlipped();
    if (!a || !b) {
        return;
    }
    const Player scorer = board_.turn();
    const MemoryBoard::Outcome outcome = board_.resolve();
    for (const int index : {*a, *b}) {
        CardView& v = views_.at(static_cast<std::size_t>(index));
        if (outcome == MemoryBoard::Outcome::Match) {
            v.leaveT = 0.0F; // fly, still face up, to the scorer's side
            v.leaveTo = scorer;
        } else {
            v.showFace = false;
            v.flipT = 0.0F;
        }
    }
    timer_ = outcome == MemoryBoard::Outcome::Match ? kLeaveSeconds : kFlipSeconds;
    phase_ = Phase::Settling;
}

void MemoryScene::nextTurn() {
    if (board_.turn() == Player::Bot) {
        phase_ = Phase::BotTurn;
        timer_ = kBotFirstDelay;
    } else {
        phase_ = Phase::PlayerTurn;
    }
}

void MemoryScene::enterGameOver() {
    phase_ = Phase::GameOver;
}

std::string MemoryScene::resultText() const {
    if (const auto winner = board_.winner()) {
        return *winner == Player::You ? "YOU WIN!" : "YOU LOST!";
    }
    return "DRAW!";
}

// ---- Update -------------------------------------------------------------------

void MemoryScene::update(float dtSeconds) {
    // Card animations and the table crossfade run in every phase.
    bool dealing = false;
    for (CardView& v : views_) {
        v.flipT = std::min(v.flipT + dtSeconds, kFlipSeconds);
        if (v.leaveT >= 0.0F) {
            v.leaveT = std::min(v.leaveT + dtSeconds, kLeaveSeconds);
        }
        if (phase_ == Phase::Dealing) {
            v.dealDelay = std::max(0.0F, v.dealDelay - dtSeconds);
            if (v.dealDelay <= 0.0F) {
                v.dealT = std::min(v.dealT + dtSeconds, kDealSeconds);
            }
            dealing = dealing || v.dealT < kDealSeconds;
        }
    }
    const float mixTarget = board_.turn() == Player::Bot ? 1.0F : 0.0F;
    const float step = dtSeconds / kTableFadeSeconds;
    tableMix_ = tableMix_ < mixTarget ? std::min(tableMix_ + step, mixTarget)
                                      : std::max(tableMix_ - step, mixTarget);

    switch (phase_) {
    case Phase::Dealing:
        if (!dealing) {
            nextTurn();
        }
        break;
    case Phase::BotTurn:
        timer_ -= dtSeconds;
        if (timer_ <= 0.0F) {
            botFlip();
        }
        break;
    case Phase::Resolving:
        timer_ -= dtSeconds;
        if (timer_ <= 0.0F) {
            settle();
        }
        break;
    case Phase::Settling:
        timer_ -= dtSeconds;
        if (timer_ <= 0.0F) {
            if (board_.isOver()) {
                enterGameOver();
            } else {
                nextTurn();
            }
        }
        break;
    case Phase::PlayerTurn:
    case Phase::GameOver:
        break;
    }
}

// ---- Rendering ----------------------------------------------------------------

void MemoryScene::drawTable(Canvas& canvas) const {
    canvas.clear(mix(theme().memBgYou, theme().memBgBot, tableMix_));
}

void MemoryScene::drawScoreboard(Canvas& canvas) const {
    canvas.fillRoundedRect(kPanelX, kPanelY, kPanelW, kPanelH, 22.0F, theme().memPanel);
    const float labelCy = kPanelY + 28.0F;
    const float scoreCy = kPanelY + 64.0F;
    canvas.textCentered("YOU", kYouCx, labelCy, 26.0F, colors::youRed);
    canvas.textCentered(std::to_string(board_.score(Player::You)), kYouCx, scoreCy, 40.0F,
                        colors::youRed);
    canvas.textCentered(label(difficulty_), layout::kWidthF / 2.0F, labelCy, 22.0F,
                        theme().mutedText);
    canvas.textCentered(std::to_string(board_.remainingPairs()) + " left", layout::kWidthF / 2.0F,
                        scoreCy, 26.0F, colors::white);
    canvas.textCentered("BOT", kBotCx, labelCy, 26.0F, colors::botCyan);
    canvas.textCentered(std::to_string(board_.score(Player::Bot)), kBotCx, scoreCy, 40.0F,
                        colors::botCyan);
}

void MemoryScene::drawTurnBanner(Canvas& canvas) const {
    if (phase_ == Phase::Dealing || phase_ == Phase::GameOver) {
        return;
    }
    const bool bot = board_.turn() == Player::Bot;
    canvas.textCentered(bot ? "BOT'S TURN" : "YOUR TURN", layout::kWidthF / 2.0F, kBannerCy, 30.0F,
                        colors::white);
}

void MemoryScene::drawCard(Canvas& canvas, int index) const {
    const CardView& v = views_.at(static_cast<std::size_t>(index));
    const MemoryBoard::Card& card = board_.card(index);
    if (card.matched && v.leaveT < 0.0F) {
        return; // collected in an earlier round of the animation: gone
    }
    if (v.leaveT >= kLeaveSeconds) {
        return;
    }

    // Where the card is: dealing in from the table's center, flying off to the
    // scorer's score, or sitting in its slot.
    float cx = v.cx;
    float cy = v.cy;
    float size = cardPx_ - (2.0F * kCardInset);
    if (phase_ == Phase::Dealing && v.dealT < kDealSeconds) {
        const float t = ease::easeOutCubic(v.dealT / kDealSeconds);
        cx = ease::lerp(layout::kWidthF / 2.0F, v.cx, t);
        cy = ease::lerp(kAreaY + (kAreaH / 2.0F), v.cy, t);
    } else if (v.leaveT >= 0.0F) {
        const float t = ease::easeInCubic(v.leaveT / kLeaveSeconds);
        cx = ease::lerp(v.cx, scoreCx(v.leaveTo), t);
        cy = ease::lerp(v.cy, kPanelY + (kPanelH / 2.0F), t);
        size *= ease::lerp(1.0F, 0.25F, t);
    }

    // A flip narrows the card to an edge and widens it again on the other side.
    const float flip = v.flipT / kFlipSeconds;
    const float width = std::abs(std::cos(flip * std::numbers::pi_v<float>));
    const bool face = flip < 0.5F ? !v.showFace : v.showFace;
    const float w = std::max(2.0F, size * width);
    const float radius = std::max(4.0F, size * 0.1F);
    const float x = cx - (w / 2.0F);
    const float y = cy - (size / 2.0F);
    if (face) {
        canvas.fillRoundedRect(x, y, w, size, radius, theme().memCardFace);
        if (width > 0.5F) {
            canvas.emojiCentered(kPictures.at(static_cast<std::size_t>(card.picture)), cx, cy,
                                 size * 0.62F);
        }
    } else {
        canvas.fillRoundedRect(x, y, w, size, radius, theme().memCardBack);
        // A ring keeps the back from reading as a blank tile.
        const float ring = size * 0.2F;
        canvas.fillCircle(cx, cy, ring * width, theme().memCardBackRing);
        canvas.fillCircle(cx, cy, (ring - (size * 0.06F)) * width, theme().memCardBack);
    }
}

void MemoryScene::drawCards(Canvas& canvas) const {
    // Flying cards are drawn last so they pass over the table, not under it.
    for (int i = 0; i < board_.cardCount(); ++i) {
        if (views_.at(static_cast<std::size_t>(i)).leaveT < 0.0F) {
            drawCard(canvas, i);
        }
    }
    for (int i = 0; i < board_.cardCount(); ++i) {
        if (views_.at(static_cast<std::size_t>(i)).leaveT >= 0.0F) {
            drawCard(canvas, i);
        }
    }
}

void MemoryScene::drawOverlay(Canvas& canvas) const {
    overlay_.render(canvas, resultText(), 560.0F, 96.0F);
    const std::string score = "YOU " + std::to_string(board_.score(Player::You)) +
                              "  \xE2\x80\x94  " + std::to_string(board_.score(Player::Bot)) +
                              " BOT";
    canvas.textCentered(score, layout::kWidthF / 2.0F, 660.0F, 36.0F, colors::white);
}

void MemoryScene::render(Canvas& canvas) {
    drawTable(canvas);
    backButton_.render(canvas);
    restartButton_.render(canvas);
    drawScoreboard(canvas);
    drawTurnBanner(canvas);
    drawCards(canvas);
    if (phase_ == Phase::GameOver) {
        drawOverlay(canvas);
    }
}

} // namespace og
