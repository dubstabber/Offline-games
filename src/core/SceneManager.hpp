#pragma once

#include "core/Scene.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace og {

// A stack of scenes. The menu sits at the bottom; opening a game pushes it on
// top, and "back" pops it off. Only the top scene receives input/update/render.
// Push/pop requests are deferred until applyPending() so a scene can safely ask
// to navigate from inside its own handleInput().
class SceneManager {
public:
    void push(std::unique_ptr<Scene> scene);
    void pop();
    void replace(std::unique_ptr<Scene> scene);

    [[nodiscard]] Scene* current() const;
    [[nodiscard]] bool empty() const { return scenes_.empty(); }

    // Apply any push/pop/replace requested during the current frame.
    void applyPending();

private:
    enum class Action : std::uint8_t { None, Push, Pop, Replace };

    std::vector<std::unique_ptr<Scene>> scenes_;
    std::unique_ptr<Scene> pendingScene_;
    Action pending_ = Action::None;
};

} // namespace og
