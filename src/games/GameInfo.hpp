#pragma once

#include "core/Scene.hpp"

#include <functional>
#include <memory>
#include <string>

namespace og {

class SceneManager;

// Catalog entry for one game. The menu renders `emoji` + `title`; selecting it
// calls `create` to build the game's Scene. Adding a game = append one of these
// in GameRegistry.cpp (see CLAUDE.md "Adding a game").
struct GameInfo {
    std::string id;    // stable identifier (e.g. for future save data)
    std::string title; // shown in the menu
    std::string emoji; // single emoji icon, drawn from a font (no images)
    std::function<std::unique_ptr<Scene>(SceneManager&)> create;
};

} // namespace og
