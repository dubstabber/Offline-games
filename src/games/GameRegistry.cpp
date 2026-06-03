#include "games/GameRegistry.hpp"

#include "games/tapmatch/TapMatchScene.hpp"
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
            .description = "Get three of your marks in a row \xE2\x80\x94 across, down, or "
                           "diagonally \xE2\x80\x94 before the bot does. Tap an empty square to "
                           "place your mark.",
            .accent = colors::youRed,
            .create = [](SceneManager& manager, Difficulty difficulty) -> std::unique_ptr<Scene> {
                return std::make_unique<TicTacToeScene>(manager, difficulty);
            },
        });
        list.push_back(GameInfo{
            .id = "tapmatch",
            .title = "Tap Match",
            .emoji = "\xF0\x9F\x8D\x89", // 🍉
            .description = "Tap to collect cards. Complete sets of 3 to clear cards. You can hold "
                           "up to 7 cards in your stack, so collect strategically.",
            .accent = colors::menuPink,
            .create = [](SceneManager& manager, Difficulty difficulty) -> std::unique_ptr<Scene> {
                return std::make_unique<TapMatchScene>(manager, difficulty);
            },
        });

        // Placeholder cards for games not built yet: a title/emoji/accent so the
        // menu has something to show and scroll, but no `create` factory, so the
        // menu marks them "SOON" and they don't open. Implement one by giving it
        // a scene + create (see "Adding a game") and dropping it from here.
        const auto placeholder = [&list](const char* id, const char* title, const char* emoji,
                                         Color accent) {
            list.push_back(GameInfo{.id = id,
                                    .title = title,
                                    .emoji = emoji,
                                    .description = "",
                                    .accent = accent,
                                    .create = nullptr});
        };
        placeholder("snake", "Snake", "\xF0\x9F\x90\x8D", colors::easyGreen);          // 🐍
        placeholder("memory", "Memory", "\xF0\x9F\xA7\xA0", colors::botCyan);          // 🧠
        placeholder("2048", "2048", "\xF0\x9F\x94\xA2", colors::mediumOrange);         // 🔢
        placeholder("mines", "Mines", "\xF0\x9F\x92\xA3", colors::menuPink);           // 💣
        placeholder("connect4", "Connect 4", "\xF0\x9F\x94\xB4", colors::menuPurple);  // 🔴
        placeholder("solitaire", "Solitaire", "\xF0\x9F\x83\x8F", colors::menuYellow); // 🃏
        placeholder("puzzle", "Puzzle", "\xF0\x9F\xA7\xA9", colors::easyGreen);        // 🧩
        placeholder("dice", "Dice", "\xF0\x9F\x8E\xB2", colors::botCyan);              // 🎲
        placeholder("pong", "Pong", "\xF0\x9F\x8F\x93", colors::hardRed);              // 🏓

        return list;
    }();
    return games;
}

} // namespace og
