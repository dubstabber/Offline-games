#pragma once

#include "core/Color.hpp"
#include "core/Scene.hpp"
#include "games/Difficulty.hpp"

#include <functional>
#include <memory>
#include <string>

namespace og {

class SceneManager;

// Catalog entry for one game. The menu renders a card (`emoji` + `title` in the
// game's `accent` color); selecting it opens the difficulty screen, where
// `description` is shown and PLAY calls `create` with the chosen Difficulty.
// Adding a game = append one of these in GameRegistry.cpp (see "Adding a game").
struct GameInfo {
    std::string id;          // stable identifier (e.g. for future save data)
    std::string title;       // shown on the menu card
    std::string emoji;       // single emoji icon, drawn from a font (no images)
    std::string description; // shown on the difficulty screen
    Color accent{};          // theme color for the card + PLAY button
    std::function<std::unique_ptr<Scene>(SceneManager&, Difficulty)> create;
};

} // namespace og
