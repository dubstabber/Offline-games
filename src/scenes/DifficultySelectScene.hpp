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
// description, a difficulty face + label, a slider you can tap or drag along
// (Easy/Medium/Hard), and a PLAY button that launches the game at the chosen
// difficulty.
class DifficultySelectScene : public Scene {
public:
    DifficultySelectScene(SceneManager& manager, GameInfo info);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    bool handleBackButton(const PointerEvent& event);
    bool handleSlider(const PointerEvent& event);
    void dragKnobTo(float x); // move the knob and snap difficulty to the nearest stop
    void setDifficulty(Difficulty difficulty);
    [[nodiscard]] float stopX(Difficulty difficulty) const; // knob centre x for a stop

    SceneManager& manager_;
    GameInfo info_;
    int stops_ = 3; // number of difficulty stops on the slider (info_.difficultyCount)
    Difficulty difficulty_ = Difficulty::Medium;
    std::string titleUpper_;
    std::vector<std::string> descLines_; // wrapped lazily on first render
    bool backPressed_ = false;
    bool draggingKnob_ = false;
    float knobX_ = 0.0F; // live knob centre while dragging; else the difficulty's stop
    Button playButton_;
};

} // namespace og
