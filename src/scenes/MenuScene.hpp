#pragma once

#include "core/Scene.hpp"
#include "games/GameInfo.hpp"

#include <vector>

namespace og {

class SceneManager;

// The home screen: a colorful 2-column grid of game cards built from
// gameRegistry(). Tapping a card opens that game's difficulty screen.
class MenuScene : public Scene {
public:
    explicit MenuScene(SceneManager& manager);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    struct Card {
        float x = 0.0F;
        float y = 0.0F;
        float w = 0.0F;
        float h = 0.0F;
        GameInfo info;
    };

    SceneManager& manager_;
    std::vector<Card> cards_;
    int pressedIndex_ = -1;
};

} // namespace og
