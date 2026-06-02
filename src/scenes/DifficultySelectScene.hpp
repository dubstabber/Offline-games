#pragma once

#include "core/Scene.hpp"
#include "games/Difficulty.hpp"
#include "games/GameInfo.hpp"
#include "ui/Button.hpp"

#include <string>
#include <vector>

namespace og {

class SceneManager;

// The screen shown after picking a game from the menu: the game's title and
// description, a difficulty face + label, a tap-to-set slider (Easy/Medium/
// Hard), and a PLAY button that launches the game at the chosen difficulty.
class DifficultySelectScene : public Scene {
public:
    DifficultySelectScene(SceneManager& manager, GameInfo info);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    bool handleBackButton(const PointerEvent& event);
    void setDifficulty(Difficulty difficulty);

    SceneManager& manager_;
    GameInfo info_;
    Difficulty difficulty_ = Difficulty::Medium;
    std::string titleUpper_;
    std::vector<std::string> descLines_; // wrapped lazily on first render
    bool backPressed_ = false;
    Button playButton_;
};

} // namespace og
