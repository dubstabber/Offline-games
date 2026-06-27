#include "games/GameRegistry.hpp"

#include "games/blockfill/BlockFillScene.hpp"
#include "games/hexanaut/HexanautScene.hpp"
#include "games/hole/HoleScene.hpp"
#include "games/minesweeper/MineSweeperScene.hpp"
#include "games/nibbles/NibblesScene.hpp"
#include "games/snake/SnakeScene.hpp"
#include "games/sokoban/SokobanScene.hpp"
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
            // Each difficulty plays its own pool of the original game's boards (see
            // TapMatchLevels), tracking its own level; PLAY launches the saved one.
            .create = [](SceneManager& manager, Difficulty difficulty) -> std::unique_ptr<Scene> {
                return std::make_unique<TapMatchScene>(manager, difficulty,
                                                       tapMatchSavedLevel(difficulty));
            },
            .currentLevel = [](Difficulty difficulty) { return tapMatchSavedLevel(difficulty); },
        });
        list.push_back(GameInfo{
            .id = "minesweeper",
            .title = "Minesweeper",
            .emoji = "\xF0\x9F\x92\xA3", // 💣
            .description = "Find all the mines by clearing every square that isn't a mine. The "
                           "numbers tell you how many adjacent squares are mines; use flags to "
                           "mark them. Each level is always fully solvable using logic!",
            .accent = colors::accent,
            .create = [](SceneManager& manager, Difficulty difficulty) -> std::unique_ptr<Scene> {
                return std::make_unique<MineSweeperScene>(manager, difficulty);
            },
        });
        list.push_back(GameInfo{
            .id = "blockfill",
            .title = "Block Fill",
            .emoji = "\xF0\x9F\x9F\xA6", // 🟦
            .description = "Draw one continuous line from the dot through every cell, covering "
                           "each cell exactly once. Drag back over the line to undo.",
            .accent = colors::accent, // light blue, like the rope
            .difficultyCount = 4,     // Easy / Medium / Hard / Very Hard
            // Each difficulty plays its own pool of the original game's boards (see
            // BlockFillLevels), tracking its own level; PLAY launches the saved one.
            .create = [](SceneManager& manager, Difficulty difficulty) -> std::unique_ptr<Scene> {
                return std::make_unique<BlockFillScene>(manager, difficulty,
                                                        blockFillSavedLevel(difficulty));
            },
            .currentLevel = [](Difficulty difficulty) { return blockFillSavedLevel(difficulty); },
        });
        list.push_back(GameInfo{
            .id = "sokoban",
            .title = "Sokoban",
            .emoji = "\xF0\x9F\x93\xA6", // 📦
            .description = "Push every crate onto a goal. Crates can only be pushed, never pulled; "
                           "use undo when a box gets trapped.",
            .accent = colors::mediumOrange,
            .create = [](SceneManager& manager, Difficulty difficulty) -> std::unique_ptr<Scene> {
                return std::make_unique<SokobanScene>(manager, difficulty,
                                                      sokobanSavedLevel(difficulty));
            },
            .currentLevel = [](Difficulty difficulty) { return sokobanSavedLevel(difficulty); },
        });
        list.push_back(GameInfo{
            .id = "nibbles",
            .title = "Nibbles",
            .emoji = "\xF0\x9F\x8D\x92", // 🍒
            .description = "Guide your worm through maze boards, collect every bonus, and avoid "
                           "walls, warps, and rival worms. Swipe or use the arrows to turn.",
            .accent = colors::easyGreen,
            .create = [](SceneManager& manager, Difficulty difficulty) -> std::unique_ptr<Scene> {
                return std::make_unique<NibblesScene>(manager, difficulty,
                                                      nibblesSavedLevel(difficulty));
            },
            .currentLevel = [](Difficulty difficulty) { return nibblesSavedLevel(difficulty); },
        });
        list.push_back(GameInfo{
            .id = "snake",
            .title = "Snake",
            .emoji = "\xF0\x9F\x90\x8D", // 🐍
            .description = "Glide around the arena, eat orbs to grow longer, and cut off rival "
                           "snakes so they crash into you. Steer toward your finger; hold BOOST "
                           "for a burst of speed that bleeds a little length. Hit another snake "
                           "or the edge and it's over.",
            .accent = colors::easyGreen,
            .create = [](SceneManager& manager, Difficulty difficulty) -> std::unique_ptr<Scene> {
                return std::make_unique<SnakeScene>(manager, difficulty);
            },
        });

        list.push_back(GameInfo{
            .id = "hexanaut",
            .title = "Hexanaut",
            .emoji = "\xF0\x9F\x9A\x80", // 🚀
            .description = "Roll out across the hex map, loop back to your own land, and claim "
                           "everything you enclosed. Cut a rival's trail before they close it to "
                           "knock them out. Own the most territory.",
            .accent = colors::menuPink,
            .create = [](SceneManager& manager, Difficulty difficulty) -> std::unique_ptr<Scene> {
                return std::make_unique<HexanautScene>(manager, difficulty);
            },
        });
        list.push_back(GameInfo{
            .id = "hole",
            .title = "Hole",
            .emoji = "\xE2\x9A\xAB", // ⚫
            .description = "Steer a growing hole through a timed city match. Swallow small props "
                           "first, grow into cars and buildings, and eat rival holes before they "
                           "outscore you.",
            .accent = colors::botCyan,
            .create = [](SceneManager& manager, Difficulty difficulty) -> std::unique_ptr<Scene> {
                return std::make_unique<HoleScene>(manager, difficulty);
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
        placeholder("memory", "Memory", "\xF0\x9F\xA7\xA0", colors::botCyan);          // 🧠
        placeholder("2048", "2048", "\xF0\x9F\x94\xA2", colors::mediumOrange);         // 🔢
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
