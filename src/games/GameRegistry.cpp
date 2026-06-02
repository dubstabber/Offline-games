#include "games/GameRegistry.hpp"

#include "games/tictactoe/TicTacToeScene.hpp"

#include <memory>

namespace og {

const std::vector<GameInfo>& gameRegistry() {
    // Built once on first use. To add a game, include its scene header above
    // and append an entry here — nothing else in the menu needs to change.
    static const std::vector<GameInfo> games = [] {
        std::vector<GameInfo> list;
        list.push_back(GameInfo{
            .id = "tictactoe",
            .title = "Tic-Tac-Toe",
            .emoji = "\xE2\x9D\x8C", // ❌
            .create = [](SceneManager& manager) -> std::unique_ptr<Scene> {
                return std::make_unique<TicTacToeScene>(manager);
            },
        });
        return list;
    }();
    return games;
}

} // namespace og
