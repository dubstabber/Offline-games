#pragma once

#include "core/Scene.hpp"
#include "ui/Button.hpp"

#include <vector>

namespace og {

class SceneManager;

// The home screen: a titled, vertical list of the games in gameRegistry().
// Selecting an entry pushes that game's scene onto the stack.
class MenuScene : public Scene {
public:
    explicit MenuScene(SceneManager& manager);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;

private:
    SceneManager& manager_;
    std::vector<Button> entries_;
};

} // namespace og
