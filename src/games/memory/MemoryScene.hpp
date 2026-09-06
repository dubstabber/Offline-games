#pragma once

#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/memory/MemoryBoard.hpp"
#include "games/memory/MemoryBot.hpp"
#include "ui/IconButton.hpp"
#include "ui/ResultOverlay.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace og {

class Canvas;
class SceneManager;

// Memory (concentration) against a bot: a table of face-down emoji cards; each
// turn flips two, a match collects the pair (and flips again), a miss turns them
// back and passes the turn. The pure rules live in MemoryBoard and the opponent
// in MemoryBot; this Scene deals the cards in, animates flips and the matched
// pairs flying to the scorer's side of the scoreboard, crossfades the table
// between the two players' colors, and shows the result overlay at the end.
class MemoryScene : public Scene {
public:
    MemoryScene(SceneManager& manager, Difficulty difficulty);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;
    // The table is static between the player's taps, so the app can idle then.
    [[nodiscard]] bool isAnimating() const override;

private:
    using Player = MemoryBoard::Player;
    // Dealing: cards fly in. PlayerTurn: taps flip cards. BotTurn: the bot flips
    // after a pause. Resolving: two cards are up for a look. Settling: they fly
    // off (match) or turn back (miss). GameOver: the result overlay.
    enum class Phase : std::uint8_t { Dealing, PlayerTurn, BotTurn, Resolving, Settling, GameOver };

    // Per-card animation state; the rules' truth stays in MemoryBoard.
    struct CardView {
        float cx = 0.0F; // table slot
        float cy = 0.0F;
        float flipT = 0.0F;    // seconds into the current flip (capped at kFlipSeconds)
        bool showFace = false; // which side the flip is turning toward
        float dealDelay = 0.0F;
        float dealT = 0.0F;   // seconds into the deal-in fly
        float leaveT = -1.0F; // < 0: on the table; else seconds into flying off
        Player leaveTo = Player::You;
    };

    static constexpr float kFlipSeconds = 0.22F;
    static constexpr float kDealSeconds = 0.5F;
    static constexpr float kLeaveSeconds = 0.5F;

    void beginRound(std::uint32_t seed);
    void layoutCards();
    void reveal(int index);
    void afterSecondFlip();
    void botFlip();
    void settle();
    void nextTurn();
    void enterGameOver();
    [[nodiscard]] int cardAt(float x, float y) const;
    [[nodiscard]] std::string resultText() const;

    void drawTable(Canvas& canvas) const;
    void drawScoreboard(Canvas& canvas) const;
    void drawTurnBanner(Canvas& canvas) const;
    void drawCards(Canvas& canvas) const;
    void drawCard(Canvas& canvas, int index) const;
    void drawOverlay(Canvas& canvas) const;

    SceneManager& manager_;
    Difficulty difficulty_;
    MemoryBoard board_;
    MemoryBot bot_;
    std::vector<CardView> views_;
    Phase phase_ = Phase::Dealing;
    float timer_ = 0.0F;    // countdown for the timed phases
    float tableMix_ = 0.0F; // 0 = the player's table color, 1 = the bot's
    float cardPx_ = 0.0F;   // cell size; the card is a little smaller

    IconButton backButton_;
    IconButton restartButton_;
    ResultOverlay overlay_;
};

} // namespace og
