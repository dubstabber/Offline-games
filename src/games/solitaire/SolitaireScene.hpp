#pragma once

#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/solitaire/KlondikeGame.hpp"
#include "ui/IconButton.hpp"
#include "ui/ResultOverlay.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace og {

class Canvas;
class SceneManager;

// Klondike solitaire. The pure rules live in KlondikeGame; this Scene lays the
// table out on the 720x1440 canvas (stock, waste fan and foundations in a top
// row, seven columns below), turns taps and drags into moves — a tap sends a
// card where it fits, a drag drops a run on a column or foundation — animates
// every card that changes place (deal-in, draws, moves, undo, snap-backs) by
// flying it from where it was, flips revealed cards, finishes a fully revealed
// table by itself, and keeps the best score per difficulty. Every difficulty
// plays deals the solver proved winnable (see SolitaireDeals).
class SolitaireScene : public Scene {
public:
    // `seed` pins the deal (tests and tools); by default every round picks one of
    // the tier's bundled winnable deals at random.
    SolitaireScene(SceneManager& manager, Difficulty difficulty,
                   std::optional<std::uint32_t> seed = std::nullopt);

    void handleInput(const PointerEvent& event) override;

    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;
    [[nodiscard]] bool isAnimating() const override;

private:
    using Move = KlondikeGame::Move;
    using Pile = KlondikeGame::Pile;
    // Dealing: cards fly out. Playing: input. Finishing: the table plays itself
    // out to the foundations. Won: the result overlay.
    enum class Phase : std::uint8_t { Dealing, Playing, Finishing, Won };

    struct Pos {
        float x = 0.0F;
        float y = 0.0F;
    };
    // Per-card animation: a flight from `from` to wherever the card now sits,
    // and a flip when a tableau card turns face up. Indexed by card id.
    struct CardAnim {
        float flyT = 1.0F; // seconds into the flight (>= kFlySeconds when settled)
        float delay = 0.0F;
        Pos from;
        float flipT = 1.0F;
        bool faceDownInFlight = false; // dealt cards travel face down, then flip
    };
    using Positions = std::array<Pos, KlondikeGame::kDeckSize>;

    static constexpr float kFlySeconds = 0.18F;
    static constexpr float kFlipSeconds = 0.2F;

    void beginRound();
    // Apply any change to the game: snapshot every card's place first, then
    // start a flight for each card that moved and a flip for each one revealed.
    template <class Fn> void animateChange(const Fn& change);
    void applyMove(const Move& move, std::optional<Pos> dragFrom);
    void tapStock();
    void undoMove();
    void handleDown(const PointerEvent& event);
    void handleUp(const PointerEvent& event);
    void dropRun();
    void snapBack();
    void finishStep();
    // Advance every flight and flip; true while any card is still in the air.
    [[nodiscard]] bool advanceAnimations(float dtSeconds);
    void enterWon();
    void saveBest();

    // ---- Geometry ----
    [[nodiscard]] static float columnX(int column);
    [[nodiscard]] float faceUpGap(int column) const;
    [[nodiscard]] Pos cardPos(PileRef pile, int index) const;
    [[nodiscard]] Positions allPositions() const;
    // The pile and card index under a point, if a run can be picked up there.
    [[nodiscard]] std::optional<Move> grabAt(float x, float y) const;
    [[nodiscard]] static std::optional<PileRef> dropTargetAt(float x, float y);
    [[nodiscard]] bool onStock(float x, float y) const;
    [[nodiscard]] static int cardId(const PlayingCard& card);
    [[nodiscard]] bool isDragged(const PlayingCard& card) const;

    // ---- Drawing ----
    void drawHud(Canvas& canvas) const;
    void drawSlots(Canvas& canvas) const;
    void drawPiles(Canvas& canvas) const;
    void drawPile(Canvas& canvas, PileRef ref) const;
    void drawFlying(Canvas& canvas) const;
    void drawDragged(Canvas& canvas) const;
    static void drawCardAt(Canvas& canvas, const PlayingCard& card, float x, float y,
                           float widthScale = 1.0F);
    [[nodiscard]] Pos animatedPos(const PlayingCard& card, Pos settled) const;
    void drawOverlay(Canvas& canvas) const;
    [[nodiscard]] std::string timeText() const;

    SceneManager& manager_;
    Difficulty difficulty_;
    int tier_;
    std::optional<std::uint32_t> fixedSeed_;
    KlondikeGame game_;
    std::array<CardAnim, KlondikeGame::kDeckSize> anims_{};
    Phase phase_ = Phase::Dealing;
    float finishTimer_ = 0.0F;
    float elapsed_ = 0.0F; // playing time in seconds
    int shownSecond_ = -1;
    bool redraw_ = false; // one extra frame when the clock ticks
    int best_;
    bool bestDirty_ = false;

    // A press on a run: it follows the finger once it has moved kTapSlop.
    bool dragging_ = false;
    bool dragMoved_ = false;
    bool stockPressed_ = false;
    Move drag_;
    int dragCount_ = 0; // cards in the dragged run
    float pressX_ = 0.0F;
    float pressY_ = 0.0F;
    float dragX_ = 0.0F; // top-left of the dragged run
    float dragY_ = 0.0F;
    float grabDx_ = 0.0F;
    float grabDy_ = 0.0F;

    IconButton backButton_;
    IconButton undoButton_;
    IconButton restartButton_;
    ResultOverlay overlay_;
};

} // namespace og
