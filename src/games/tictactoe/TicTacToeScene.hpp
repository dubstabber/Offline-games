#pragma once

#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/tictactoe/TicTacToeBoard.hpp"
#include "games/tictactoe/TicTacToeBot.hpp"
#include "ui/Button.hpp"

#include <cstdint>
#include <string>

namespace og {

class SceneManager;

// Renders a Tic-Tac-Toe board (a code-drawn `#` grid with red X / cyan O marks)
// and pits the player (YOU = X) against a bot (BOT = O) at the chosen
// difficulty. A scoreboard tracks the running YOU vs BOT score; finishing a
// round shows a result overlay with Home and Play Again buttons.
class TicTacToeScene : public Scene {
public:
    TicTacToeScene(SceneManager& manager, Difficulty difficulty);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    // PlayerTurn: accept taps. BotThinking: short pause, then the bot moves.
    // GameOver: the result overlay is shown and only its buttons respond.
    enum class Phase : std::uint8_t { PlayerTurn, BotThinking, GameOver };

    bool handleBackButton(const PointerEvent& event);
    void beginRound();
    void enterGameOver();
    [[nodiscard]] std::string resultText() const;

    static void drawBackButton(Canvas& canvas);
    void drawScoreboard(Canvas& canvas) const;
    static void drawGrid(Canvas& canvas);
    void drawMarks(Canvas& canvas) const;
    void drawOverlay(Canvas& canvas) const;

    SceneManager& manager_;
    TicTacToeBoard board_;
    TicTacToeBot bot_;
    Difficulty difficulty_;
    Phase phase_ = Phase::PlayerTurn;
    float botTimer_ = 0.0F;
    int youScore_ = 0;
    int botScore_ = 0;
    bool backPressed_ = false;
    Button homeButton_;
    Button playAgainButton_;
};

} // namespace og
