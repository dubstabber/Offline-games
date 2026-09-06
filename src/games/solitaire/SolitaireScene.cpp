#include "games/solitaire/SolitaireScene.hpp"

#include "core/Canvas.hpp"
#include "core/Easing.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"
#include "games/solitaire/SolitaireDeals.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <string>
#include <utility>

namespace og {
namespace {

using Kind = PileRef::Kind;

// ---- Top bar ------------------------------------------------------------------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;
constexpr float kUndoCx = layout::kWidthF - 220.0F;
constexpr float kRestartCx = layout::kWidthF - 92.0F;
constexpr const char* kUndoGlyph = "\xE2\x86\xA9";        // ↩
constexpr const char* kRestartGlyph = "\xF0\x9F\x94\x84"; // 🔄
constexpr float kHudLabelCy = 182.0F;
constexpr float kHudValueCy = 218.0F;

// ---- Table ------------------------------------------------------------------
constexpr float kMargin = 12.0F;
constexpr float kColumnPitch = (layout::kWidthF - (2.0F * kMargin)) / 7.0F;
constexpr float kCardW = 90.0F;
constexpr float kCardH = 126.0F;
constexpr float kCardRadius = 8.0F;
constexpr float kTopRowY = 262.0F;
constexpr float kTableauY = 424.0F;
constexpr float kHiddenGap = 14.0F; // face-down cards peek this much
constexpr float kFaceUpGapMax = 32.0F;
constexpr float kWasteFanDx = 40.0F; // draw-three fan spacing
constexpr int kStockColumn = 0;
constexpr int kWasteColumn = 1;
constexpr int kFirstFoundationColumn = 3;
constexpr float kTapSlop = 12.0F;

// ---- Timing -------------------------------------------------------------------
constexpr float kDealStagger = 0.025F;
constexpr float kFinishStep = 0.1F; // between auto-complete plays

// ---- Result overlay -----------------------------------------------------------
constexpr float kButtonRowY = 820.0F;

constexpr Color kBlackInk = rgb(30, 30, 36);
constexpr Color kRedInk = rgb(214, 42, 48);
constexpr Color kCardEdge = rgb(196, 196, 190);

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

[[nodiscard]] int& bestField(Settings& s, Difficulty difficulty) {
    switch (difficulty) {
    case Difficulty::Easy:
        return s.solitaireBestEasy;
    case Difficulty::Hard:
    case Difficulty::VeryHard:
        return s.solitaireBestHard;
    case Difficulty::Medium:
        break;
    }
    return s.solitaireBestMedium;
}

[[nodiscard]] const char* rankText(int rank) {
    switch (rank) {
    case 1:
        return "A";
    case 11:
        return "J";
    case 12:
        return "Q";
    case 13:
        return "K";
    default:
        break;
    }
    static const std::array<const char*, 11> kDigits{"",  "",  "2", "3", "4", "5",
                                                     "6", "7", "8", "9", "10"};
    return kDigits.at(static_cast<std::size_t>(std::clamp(rank, 2, 10)));
}

// The four suits drawn from circles, triangles and a stem, centered on (cx, cy)
// and about `size` tall.
void drawSuit(Canvas& canvas, Suit suit, float cx, float cy, float size, Color color) {
    const auto tri = [&](float x1, float y1, float x2, float y2, float x3, float y3) {
        const std::array<Canvas::Vertex, 3> v{{{.x = x1, .y = y1, .color = color},
                                               {.x = x2, .y = y2, .color = color},
                                               {.x = x3, .y = y3, .color = color}}};
        canvas.fillConvexPolygon(v);
    };
    const auto stem = [&] {
        tri(cx - (0.22F * size), cy + (0.5F * size), cx + (0.22F * size), cy + (0.5F * size), cx,
            cy + (0.05F * size));
    };
    switch (suit) {
    case Suit::Diamonds: {
        const std::array<Canvas::Vertex, 4> v{
            {{.x = cx, .y = cy - (0.5F * size), .color = color},
             {.x = cx + (0.36F * size), .y = cy, .color = color},
             {.x = cx, .y = cy + (0.5F * size), .color = color},
             {.x = cx - (0.36F * size), .y = cy, .color = color}}};
        canvas.fillConvexPolygon(v);
        break;
    }
    case Suit::Hearts:
        canvas.fillCircle(cx - (0.24F * size), cy - (0.18F * size), 0.26F * size, color);
        canvas.fillCircle(cx + (0.24F * size), cy - (0.18F * size), 0.26F * size, color);
        tri(cx - (0.48F * size), cy - (0.1F * size), cx + (0.48F * size), cy - (0.1F * size), cx,
            cy + (0.5F * size));
        break;
    case Suit::Spades:
        canvas.fillCircle(cx - (0.24F * size), cy + (0.06F * size), 0.26F * size, color);
        canvas.fillCircle(cx + (0.24F * size), cy + (0.06F * size), 0.26F * size, color);
        tri(cx - (0.48F * size), cy + (0.0F * size), cx + (0.48F * size), cy + (0.0F * size), cx,
            cy - (0.5F * size));
        stem();
        break;
    case Suit::Clubs:
        canvas.fillCircle(cx, cy - (0.24F * size), 0.23F * size, color);
        canvas.fillCircle(cx - (0.25F * size), cy + (0.06F * size), 0.23F * size, color);
        canvas.fillCircle(cx + (0.25F * size), cy + (0.06F * size), 0.23F * size, color);
        stem();
        break;
    }
}

[[nodiscard]] std::uint32_t randomPick() {
    return std::random_device{}();
}

} // namespace

// ---- Setup --------------------------------------------------------------------

SolitaireScene::SolitaireScene(SceneManager& manager, Difficulty difficulty,
                               std::optional<std::uint32_t> seed)
    : manager_(manager), difficulty_(difficulty), tier_(difficultyToIndex(difficulty)),
      fixedSeed_(seed), game_(klondikeRules(tier_), seed.value_or(0)),
      best_(bestField(settings(), difficulty)),
      backButton_(IconButton::Icon::Back, kBackCx, kBackCy, kBackRadius),
      undoButton_(IconButton::Icon::Glyph, kUndoCx, kBackCy, kBackRadius),
      restartButton_(IconButton::Icon::Glyph, kRestartCx, kBackCy, kBackRadius),
      overlay_(colors::menuYellow, kBlackInk, kButtonRowY) {
    backButton_.setOnTap([this] {
        saveBest();
        manager_.pop();
    });
    undoButton_.setGlyph(kUndoGlyph, 52.0F);
    undoButton_.setOnTap([this] { undoMove(); });
    restartButton_.setGlyph(kRestartGlyph, 50.0F);
    restartButton_.setOnTap([this] { beginRound(); });
    overlay_.setOnHome([this] {
        saveBest();
        manager_.popToRoot();
    });
    overlay_.setActionLabel("NEW DEAL");
    overlay_.setOnAction([this] { beginRound(); });
    beginRound();
}

void SolitaireScene::beginRound() {
    game_ = KlondikeGame(klondikeRules(tier_),
                         fixedSeed_.value_or(solitaireDealSeed(tier_, randomPick())));
    // Every card starts on the stock and flies out face down to its place, one
    // after another; each column's top card flips over once it lands.
    const Pos stockPos = cardPos(PileRef{.kind = Kind::Stock, .index = 0}, 0);
    int order = 0;
    for (CardAnim& a : anims_) {
        a = CardAnim{};
    }
    for (int col = 0; col < KlondikeGame::kTableauCount; ++col) {
        for (const PlayingCard& card : game_.tableau(col)) {
            CardAnim& a = anims_.at(static_cast<std::size_t>(cardId(card)));
            a.from = stockPos;
            a.flyT = 0.0F;
            a.delay = static_cast<float>(order++) * kDealStagger;
            a.faceDownInFlight = true;
        }
    }
    phase_ = Phase::Dealing;
    elapsed_ = 0.0F;
    shownSecond_ = -1;
    finishTimer_ = 0.0F;
    dragging_ = false;
    dragMoved_ = false;
    stockPressed_ = false;
}

int SolitaireScene::cardId(const PlayingCard& card) {
    return (static_cast<int>(card.suit) * kRankCount) + (card.rank - 1);
}

bool SolitaireScene::isAnimating() const {
    if (phase_ == Phase::Dealing || phase_ == Phase::Finishing || dragging_ || redraw_) {
        return true;
    }
    return std::ranges::any_of(
        anims_, [](const CardAnim& a) { return a.flyT < kFlySeconds || a.flipT < kFlipSeconds; });
}

// ---- Geometry -----------------------------------------------------------------

float SolitaireScene::columnX(int column) {
    return kMargin + (static_cast<float>(column) * kColumnPitch) + ((kColumnPitch - kCardW) / 2.0F);
}

float SolitaireScene::faceUpGap(int column) const {
    // Long columns squeeze their face-up spacing so the run stays on screen.
    const Pile& pile = game_.tableau(column);
    int hidden = 0;
    for (const PlayingCard& c : pile) {
        hidden += c.faceUp ? 0 : 1;
    }
    const int faceUp = static_cast<int>(pile.size()) - hidden;
    if (faceUp <= 1) {
        return kFaceUpGapMax;
    }
    const float available =
        layout::kHeightF - kTableauY - kCardH - 16.0F - (static_cast<float>(hidden) * kHiddenGap);
    return std::clamp(available / static_cast<float>(faceUp - 1), 12.0F, kFaceUpGapMax);
}

SolitaireScene::Pos SolitaireScene::cardPos(PileRef pile, int index) const {
    switch (pile.kind) {
    case Kind::Stock:
        return {.x = columnX(kStockColumn), .y = kTopRowY};
    case Kind::Waste: {
        // The last drawCount cards fan out to the right; the top card is rightmost.
        const int size = static_cast<int>(game_.waste().size());
        const int fanned = std::min(game_.rules().drawCount, size);
        const int slot = std::max(0, index - (size - fanned));
        return {.x = columnX(kWasteColumn) + (static_cast<float>(slot) * kWasteFanDx),
                .y = kTopRowY};
    }
    case Kind::Foundation:
        return {.x = columnX(kFirstFoundationColumn + pile.index), .y = kTopRowY};
    case Kind::Tableau:
        break;
    }
    const Pile& column = game_.tableau(pile.index);
    const float gap = faceUpGap(pile.index);
    float y = kTableauY;
    for (int i = 0; i < index && std::cmp_less(i, column.size()); ++i) {
        y += column.at(static_cast<std::size_t>(i)).faceUp ? gap : kHiddenGap;
    }
    return {.x = columnX(pile.index), .y = y};
}

SolitaireScene::Positions SolitaireScene::allPositions() const {
    Positions out{};
    const auto place = [&](PileRef ref, const Pile& pile) {
        for (int i = 0; std::cmp_less(i, pile.size()); ++i) {
            out.at(static_cast<std::size_t>(cardId(pile.at(static_cast<std::size_t>(i))))) =
                cardPos(ref, i);
        }
    };
    place(PileRef{.kind = Kind::Stock, .index = 0}, game_.stock());
    place(PileRef{.kind = Kind::Waste, .index = 0}, game_.waste());
    for (int f = 0; f < KlondikeGame::kFoundationCount; ++f) {
        place(PileRef{.kind = Kind::Foundation, .index = f}, game_.foundation(f));
    }
    for (int col = 0; col < KlondikeGame::kTableauCount; ++col) {
        place(PileRef{.kind = Kind::Tableau, .index = col}, game_.tableau(col));
    }
    return out;
}

bool SolitaireScene::onStock(float x, float y) const {
    const Pos p = cardPos(PileRef{.kind = Kind::Stock, .index = 0}, 0);
    return x >= p.x - 8.0F && x < p.x + kCardW + 8.0F && y >= p.y - 8.0F && y < p.y + kCardH + 8.0F;
}

std::optional<SolitaireScene::Move> SolitaireScene::grabAt(float x, float y) const {
    const auto inside = [&](Pos p) {
        return x >= p.x && x < p.x + kCardW && y >= p.y && y < p.y + kCardH;
    };
    // Waste and foundation tops.
    if (!game_.waste().empty()) {
        const int top = static_cast<int>(game_.waste().size()) - 1;
        const PileRef ref{.kind = Kind::Waste, .index = 0};
        if (inside(cardPos(ref, top))) {
            return Move{.from = ref, .fromIndex = top, .to = {}};
        }
    }
    for (int f = 0; f < KlondikeGame::kFoundationCount; ++f) {
        const Pile& pile = game_.foundation(f);
        const PileRef ref{.kind = Kind::Foundation, .index = f};
        if (!pile.empty() && inside(cardPos(ref, static_cast<int>(pile.size()) - 1))) {
            return Move{.from = ref, .fromIndex = static_cast<int>(pile.size()) - 1, .to = {}};
        }
    }
    // Tableau: the topmost card under the finger, if it is face up.
    for (int col = 0; col < KlondikeGame::kTableauCount; ++col) {
        const Pile& pile = game_.tableau(col);
        const PileRef ref{.kind = Kind::Tableau, .index = col};
        for (int i = static_cast<int>(pile.size()) - 1; i >= 0; --i) {
            if (inside(cardPos(ref, i))) {
                if (pile.at(static_cast<std::size_t>(i)).faceUp) {
                    return Move{.from = ref, .fromIndex = i, .to = {}};
                }
                break;
            }
        }
    }
    return std::nullopt;
}

std::optional<PileRef> SolitaireScene::dropTargetAt(float x, float y) {
    if (x < kMargin || x >= layout::kWidthF - kMargin) {
        return std::nullopt;
    }
    const int column = std::clamp(static_cast<int>((x - kMargin) / kColumnPitch), 0, 6);
    if (y < kTableauY - 20.0F) {
        if (column >= kFirstFoundationColumn && y >= kTopRowY - 20.0F) {
            return PileRef{.kind = Kind::Foundation, .index = column - kFirstFoundationColumn};
        }
        return std::nullopt;
    }
    return PileRef{.kind = Kind::Tableau, .index = column};
}

bool SolitaireScene::isDragged(const PlayingCard& card) const {
    if (!dragging_ || !dragMoved_) {
        return false;
    }
    const Pile& pile = game_.pile(drag_.from);
    for (int i = drag_.fromIndex; std::cmp_less(i, pile.size()); ++i) {
        if (sameCard(pile.at(static_cast<std::size_t>(i)), card)) {
            return true;
        }
    }
    return false;
}

// ---- Changes ------------------------------------------------------------------

template <class Fn> void SolitaireScene::animateChange(const Fn& change) {
    const Positions before = allPositions();
    std::array<bool, KlondikeGame::kDeckSize> wasUp{};
    for (int col = 0; col < KlondikeGame::kTableauCount; ++col) {
        for (const PlayingCard& c : game_.tableau(col)) {
            wasUp.at(static_cast<std::size_t>(cardId(c))) = c.faceUp;
        }
    }
    change();
    const Positions after = allPositions();
    for (std::size_t id = 0; id < after.size(); ++id) {
        const Pos& a = before.at(id);
        const Pos& b = after.at(id);
        if (std::abs(a.x - b.x) > 0.5F || std::abs(a.y - b.y) > 0.5F) {
            CardAnim& anim = anims_.at(id);
            anim.from = animatedPos(PlayingCard{.rank = static_cast<int>(id % kRankCount) + 1,
                                                .suit = static_cast<Suit>(id / kRankCount)},
                                    a);
            anim.flyT = 0.0F;
            anim.delay = 0.0F;
        }
    }
    for (int col = 0; col < KlondikeGame::kTableauCount; ++col) {
        for (const PlayingCard& c : game_.tableau(col)) {
            const auto id = static_cast<std::size_t>(cardId(c));
            if (c.faceUp && !wasUp.at(id) && before.at(id).x == after.at(id).x &&
                before.at(id).y == after.at(id).y) {
                anims_.at(id).flipT = 0.0F;
            }
        }
    }
}

void SolitaireScene::applyMove(const Move& move, std::optional<Pos> dragFrom) {
    // A dragged run takes off from under the finger rather than its old slot.
    const int count = static_cast<int>(game_.pile(move.from).size()) - move.fromIndex;
    const float gap = move.from.kind == Kind::Tableau ? faceUpGap(move.from.index) : 0.0F;
    animateChange([&] { game_.move(move); });
    if (dragFrom) {
        const Pile& dst = game_.pile(move.to);
        for (int k = 0; k < count; ++k) {
            const std::size_t index =
                dst.size() - static_cast<std::size_t>(count) + static_cast<std::size_t>(k);
            CardAnim& a = anims_.at(static_cast<std::size_t>(cardId(dst.at(index))));
            a.from = Pos{.x = dragFrom->x, .y = dragFrom->y + (static_cast<float>(k) * gap)};
            a.flyT = 0.0F;
        }
    }
}

void SolitaireScene::tapStock() {
    if (!game_.canDraw()) {
        return;
    }
    animateChange([&] { game_.draw(); });
}

void SolitaireScene::undoMove() {
    if (phase_ != Phase::Playing || dragging_ || !game_.canUndo()) {
        return;
    }
    animateChange([&] { game_.undo(); });
}

void SolitaireScene::finishStep() {
    animateChange([&] { game_.autoCompleteStep(); });
}

void SolitaireScene::enterWon() {
    phase_ = Phase::Won;
    if (game_.score() > best_) {
        best_ = game_.score();
        bestDirty_ = true;
    }
    saveBest();
}

void SolitaireScene::saveBest() {
    if (!bestDirty_) {
        return;
    }
    Settings& s = settings();
    bestField(s, difficulty_) = best_;
    saveSettings(s);
    bestDirty_ = false;
}

// ---- Input --------------------------------------------------------------------

void SolitaireScene::handleInput(const PointerEvent& event) {
    if (backButton_.handleInput(event)) {
        return;
    }
    if (phase_ == Phase::Won) {
        overlay_.handleInput(event);
        return;
    }
    if (!dragging_ && (undoButton_.handleInput(event) || restartButton_.handleInput(event))) {
        return;
    }
    if (phase_ != Phase::Playing) {
        return;
    }
    switch (event.phase) {
    case PointerEvent::Phase::Down:
        handleDown(event);
        break;
    case PointerEvent::Phase::Move:
        if (dragging_) {
            dragX_ = event.x - grabDx_;
            dragY_ = event.y - grabDy_;
            if (!dragMoved_ && std::hypot(event.x - pressX_, event.y - pressY_) > kTapSlop) {
                dragMoved_ = true;
            }
        }
        break;
    case PointerEvent::Phase::Up:
        handleUp(event);
        break;
    }
}

void SolitaireScene::handleDown(const PointerEvent& event) {
    pressX_ = event.x;
    pressY_ = event.y;
    if (onStock(event.x, event.y)) {
        stockPressed_ = true;
        return;
    }
    const auto grab = grabAt(event.x, event.y);
    if (!grab) {
        return;
    }
    drag_ = *grab;
    dragCount_ = static_cast<int>(game_.pile(drag_.from).size()) - drag_.fromIndex;
    const Pos p = cardPos(drag_.from, drag_.fromIndex);
    grabDx_ = event.x - p.x;
    grabDy_ = event.y - p.y;
    dragX_ = p.x;
    dragY_ = p.y;
    dragging_ = true;
    dragMoved_ = false;
}

void SolitaireScene::handleUp(const PointerEvent& event) {
    if (stockPressed_) {
        stockPressed_ = false;
        if (onStock(event.x, event.y)) {
            tapStock();
        }
        return;
    }
    if (!dragging_) {
        return;
    }
    dragging_ = false;
    if (!dragMoved_) {
        // A tap: send the run wherever it fits.
        if (const auto move = game_.autoMove(drag_.from, drag_.fromIndex)) {
            applyMove(*move, std::nullopt);
        }
        return;
    }
    dropRun();
}

void SolitaireScene::dropRun() {
    // The pile under the dragged card's center takes the run, if it may.
    const auto target = dropTargetAt(dragX_ + (kCardW / 2.0F), dragY_ + (kCardH / 2.0F));
    Move move = drag_;
    if (target) {
        move.to = *target;
    }
    if (target && game_.canMove(move)) {
        applyMove(move, Pos{.x = dragX_, .y = dragY_});
    } else {
        snapBack();
    }
}

void SolitaireScene::snapBack() {
    // The run flies home from where it was dropped.
    const Pile& pile = game_.pile(drag_.from);
    const float gap = drag_.from.kind == Kind::Tableau ? faceUpGap(drag_.from.index) : 0.0F;
    for (int i = drag_.fromIndex; std::cmp_less(i, pile.size()); ++i) {
        CardAnim& a =
            anims_.at(static_cast<std::size_t>(cardId(pile.at(static_cast<std::size_t>(i)))));
        a.from = Pos{.x = dragX_, .y = dragY_ + (static_cast<float>(i - drag_.fromIndex) * gap)};
        a.flyT = 0.0F;
        a.delay = 0.0F;
    }
}

// ---- Update -------------------------------------------------------------------

bool SolitaireScene::advanceAnimations(float dtSeconds) {
    bool flying = false;
    for (CardAnim& a : anims_) {
        if (a.flyT < kFlySeconds) {
            if (a.delay > 0.0F) {
                a.delay = std::max(0.0F, a.delay - dtSeconds);
            } else {
                a.flyT = std::min(a.flyT + dtSeconds, kFlySeconds);
            }
            if (a.flyT >= kFlySeconds && a.faceDownInFlight) {
                a.faceDownInFlight = false;
                a.flipT = 0.0F; // a dealt top card turns over as it lands
            }
            flying = true;
        }
        a.flipT = std::min(a.flipT + dtSeconds, kFlipSeconds);
    }
    return flying;
}

void SolitaireScene::update(float dtSeconds) {
    const bool flying = advanceAnimations(dtSeconds);
    redraw_ = false;
    switch (phase_) {
    case Phase::Dealing:
        if (!flying) {
            phase_ = Phase::Playing;
        }
        break;
    case Phase::Playing: {
        elapsed_ += dtSeconds;
        const int second = static_cast<int>(elapsed_);
        if (second != shownSecond_) {
            shownSecond_ = second;
            redraw_ = true;
        }
        if (game_.isWon()) {
            phase_ = Phase::Finishing; // let the last flight land before the overlay
        } else if (!dragging_ && game_.canAutoComplete()) {
            phase_ = Phase::Finishing;
            finishTimer_ = kFinishStep;
        }
        break;
    }
    case Phase::Finishing:
        if (game_.isWon()) {
            if (!flying) {
                enterWon();
            }
            break;
        }
        finishTimer_ -= dtSeconds;
        if (finishTimer_ <= 0.0F) {
            finishStep();
            finishTimer_ = kFinishStep;
        }
        break;
    case Phase::Won:
        break;
    }
}

// ---- Drawing ------------------------------------------------------------------

std::string SolitaireScene::timeText() const {
    const int total = static_cast<int>(elapsed_);
    const int minutes = total / 60;
    const int seconds = total % 60;
    return std::to_string(minutes) + (seconds < 10 ? ":0" : ":") + std::to_string(seconds);
}

void SolitaireScene::drawHud(Canvas& canvas) const {
    backButton_.render(canvas);
    undoButton_.render(canvas);
    restartButton_.render(canvas);
    const Color labelInk = rgb(210, 230, 214);
    const auto stat = [&](float cx, const char* label, const std::string& value) {
        canvas.textCentered(label, cx, kHudLabelCy, 20.0F, labelInk);
        canvas.textCentered(value, cx, kHudValueCy, 34.0F, colors::white);
    };
    stat(layout::kWidthF * 0.2F, "SCORE", std::to_string(game_.score()));
    stat(layout::kWidthF * 0.5F, "TIME", timeText());
    stat(layout::kWidthF * 0.8F, "MOVES", std::to_string(game_.moves()));
    canvas.textCentered(label(difficulty_), layout::kWidthF / 2.0F, 118.0F, 22.0F, labelInk);
    if (game_.rules().maxPasses > 0) {
        const int left = game_.rules().maxPasses - game_.passes();
        canvas.textCentered(std::to_string(left) + (left == 1 ? " pass left" : " passes left"),
                            layout::kWidthF / 2.0F, 88.0F, 20.0F, labelInk);
    }
}

void SolitaireScene::drawSlots(Canvas& canvas) const {
    const Color slot = theme().solSlot;
    // Stock: a ring that reads "turn over" once the stock is spent.
    const Pos stock = cardPos(PileRef{.kind = Kind::Stock, .index = 0}, 0);
    canvas.fillRoundedRect(stock.x, stock.y, kCardW, kCardH, kCardRadius, slot);
    if (game_.stock().empty()) {
        const float cx = stock.x + (kCardW / 2.0F);
        const float cy = stock.y + (kCardH / 2.0F);
        const Color ring = game_.canRecycle() ? rgb(255, 255, 255, 160) : rgb(255, 255, 255, 60);
        canvas.fillCircle(cx, cy, 24.0F, ring);
        canvas.fillCircle(cx, cy, 16.0F, theme().solTable);
    }
    const Pos waste = cardPos(PileRef{.kind = Kind::Waste, .index = 0}, 0);
    canvas.fillRoundedRect(waste.x, waste.y, kCardW, kCardH, kCardRadius, slot);
    for (int f = 0; f < KlondikeGame::kFoundationCount; ++f) {
        const Pos p = cardPos(PileRef{.kind = Kind::Foundation, .index = f}, 0);
        canvas.fillRoundedRect(p.x, p.y, kCardW, kCardH, kCardRadius, slot);
        drawSuit(canvas, KlondikeGame::foundationSuit(f), p.x + (kCardW / 2.0F),
                 p.y + (kCardH / 2.0F), 40.0F, rgb(255, 255, 255, 70));
    }
    for (int col = 0; col < KlondikeGame::kTableauCount; ++col) {
        canvas.fillRoundedRect(columnX(col), kTableauY, kCardW, kCardH, kCardRadius, slot);
    }
}

SolitaireScene::Pos SolitaireScene::animatedPos(const PlayingCard& card, Pos settled) const {
    const CardAnim& a = anims_.at(static_cast<std::size_t>(cardId(card)));
    if (a.flyT >= kFlySeconds) {
        return settled;
    }
    const float t = ease::easeOutCubic(a.flyT / kFlySeconds);
    return {.x = ease::lerp(a.from.x, settled.x, t), .y = ease::lerp(a.from.y, settled.y, t)};
}

void SolitaireScene::drawCardAt(Canvas& canvas, const PlayingCard& card, float x, float y,
                                float widthScale) {
    const float w = std::max(2.0F, kCardW * widthScale);
    const float left = x + ((kCardW - w) / 2.0F);
    if (!card.faceUp) {
        canvas.fillRoundedRect(left, y, w, kCardH, kCardRadius, theme().solCardBack);
        if (widthScale > 0.3F) {
            const float inset = 7.0F;
            canvas.fillRoundedRect(left + inset, y + inset, w - (2.0F * inset),
                                   kCardH - (2.0F * inset), kCardRadius - 3.0F,
                                   theme().solCardBackInner);
            canvas.fillRoundedRect(left + inset + 4.0F, y + inset + 4.0F, w - (2.0F * inset) - 8.0F,
                                   kCardH - (2.0F * inset) - 8.0F, kCardRadius - 5.0F,
                                   theme().solCardBack);
            drawSuit(canvas, Suit::Diamonds, x + (kCardW / 2.0F), y + (kCardH / 2.0F),
                     22.0F * widthScale, theme().solCardBackInner);
        }
        return;
    }
    canvas.fillRoundedRect(left, y, w, kCardH, kCardRadius, kCardEdge);
    canvas.fillRoundedRect(left + 1.5F, y + 1.5F, w - 3.0F, kCardH - 3.0F, kCardRadius - 1.0F,
                           theme().solCardFace);
    if (widthScale < 0.5F) {
        return;
    }
    const Color ink = isRed(card.suit) ? kRedInk : kBlackInk;
    // Corner index (rank + small suit) stays visible when the next card overlaps.
    const float rankX = left + 7.0F;
    canvas.text(rankText(card.rank), rankX, y + 3.0F, 27.0F, ink);
    const float rankW = card.rank == 10 ? 30.0F : 17.0F;
    drawSuit(canvas, card.suit, rankX + rankW + 11.0F, y + 17.0F, 17.0F, ink);
    drawSuit(canvas, card.suit, x + (kCardW / 2.0F), y + (kCardH * 0.62F), 44.0F, ink);
}

void SolitaireScene::drawPile(Canvas& canvas, PileRef ref) const {
    const Pile& pile = game_.pile(ref);
    for (int i = 0; std::cmp_less(i, pile.size()); ++i) {
        const PlayingCard& card = pile.at(static_cast<std::size_t>(i));
        const CardAnim& a = anims_.at(static_cast<std::size_t>(cardId(card)));
        if (a.flyT < kFlySeconds || isDragged(card)) {
            continue; // drawn on top by drawFlying / drawDragged
        }
        const Pos p = cardPos(ref, i);
        if (a.flipT < kFlipSeconds && card.faceUp) {
            const float flip = a.flipT / kFlipSeconds;
            PlayingCard shown = card;
            shown.faceUp = flip >= 0.5F;
            drawCardAt(canvas, shown, p.x, p.y,
                       std::abs(std::cos(flip * std::numbers::pi_v<float>)));
        } else {
            drawCardAt(canvas, card, p.x, p.y);
        }
    }
}

void SolitaireScene::drawPiles(Canvas& canvas) const {
    drawPile(canvas, PileRef{.kind = Kind::Stock, .index = 0});
    drawPile(canvas, PileRef{.kind = Kind::Waste, .index = 0});
    for (int f = 0; f < KlondikeGame::kFoundationCount; ++f) {
        drawPile(canvas, PileRef{.kind = Kind::Foundation, .index = f});
    }
    for (int col = 0; col < KlondikeGame::kTableauCount; ++col) {
        drawPile(canvas, PileRef{.kind = Kind::Tableau, .index = col});
    }
}

void SolitaireScene::drawFlying(Canvas& canvas) const {
    // Flying cards are drawn after every pile so they pass over the table; the
    // most recently launched (lowest flyT) last, so a run keeps its order.
    const auto drawFrom = [&](PileRef ref, const Pile& pile) {
        for (int i = 0; std::cmp_less(i, pile.size()); ++i) {
            const PlayingCard& card = pile.at(static_cast<std::size_t>(i));
            const CardAnim& a = anims_.at(static_cast<std::size_t>(cardId(card)));
            if (a.flyT >= kFlySeconds || isDragged(card)) {
                continue;
            }
            const Pos p = animatedPos(card, cardPos(ref, i));
            PlayingCard shown = card;
            shown.faceUp = card.faceUp && !a.faceDownInFlight;
            drawCardAt(canvas, shown, p.x, p.y);
        }
    };
    drawFrom(PileRef{.kind = Kind::Stock, .index = 0}, game_.stock());
    drawFrom(PileRef{.kind = Kind::Waste, .index = 0}, game_.waste());
    for (int f = 0; f < KlondikeGame::kFoundationCount; ++f) {
        drawFrom(PileRef{.kind = Kind::Foundation, .index = f}, game_.foundation(f));
    }
    for (int col = 0; col < KlondikeGame::kTableauCount; ++col) {
        drawFrom(PileRef{.kind = Kind::Tableau, .index = col}, game_.tableau(col));
    }
}

void SolitaireScene::drawDragged(Canvas& canvas) const {
    if (!dragging_ || !dragMoved_) {
        return;
    }
    const Pile& pile = game_.pile(drag_.from);
    const float gap = drag_.from.kind == Kind::Tableau ? faceUpGap(drag_.from.index) : 0.0F;
    for (int i = drag_.fromIndex; std::cmp_less(i, pile.size()); ++i) {
        drawCardAt(canvas, pile.at(static_cast<std::size_t>(i)), dragX_,
                   dragY_ + (static_cast<float>(i - drag_.fromIndex) * gap));
    }
}

void SolitaireScene::drawOverlay(Canvas& canvas) const {
    overlay_.render(canvas, "YOU WIN!", 520.0F, 88.0F);
    canvas.textCentered("SCORE  " + std::to_string(game_.score()), layout::kWidthF / 2.0F, 616.0F,
                        40.0F, colors::white);
    canvas.textCentered(timeText() + "   " + std::to_string(game_.moves()) + " moves",
                        layout::kWidthF / 2.0F, 672.0F, 28.0F, colors::white);
    canvas.textCentered("BEST  " + std::to_string(best_), layout::kWidthF / 2.0F, 724.0F, 32.0F,
                        colors::menuYellow);
}

void SolitaireScene::render(Canvas& canvas) {
    canvas.clear(theme().solTable);
    drawHud(canvas);
    drawSlots(canvas);
    drawPiles(canvas);
    drawFlying(canvas);
    drawDragged(canvas);
    if (phase_ == Phase::Won) {
        drawOverlay(canvas);
    }
}

} // namespace og
