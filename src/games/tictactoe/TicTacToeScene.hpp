#pragma once

#include "core/Scene.hpp"
#include "games/tictactoe/TicTacToeBoard.hpp"
#include "ui/Button.hpp"

namespace og {

class SceneManager;

// Renders a Tic-Tac-Toe board with ❌/⭕ emoji and a status line, and turns
// taps into moves on the pure TicTacToeBoard. This is the reference game: it
// shows the full pattern (logic class + Scene + registry entry) to copy when
// adding new games.
class TicTacToeScene : public Scene {
public:
    explicit TicTacToeScene(SceneManager& manager);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    [[nodiscard]] std::string statusText() const;

    SceneManager& manager_;
    TicTacToeBoard board_;
    Button backButton_;
    Button newGameButton_;
};

} // namespace og
