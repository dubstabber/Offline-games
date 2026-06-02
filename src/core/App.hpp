#pragma once

#include "core/Canvas.hpp"
#include "core/FontManager.hpp"
#include "core/SceneManager.hpp"
#include "core/Sdl.hpp"

#include <memory>

namespace og {

// Owns the SDL window/renderer and runs the game loop. Construction does no SDL
// work; call init() once (it reports failure) and then run(). The window uses a
// fixed logical canvas (Layout) scaled with letterboxing, so the same build
// fills the PinePhone screen and runs in a small desktop window for development.
class App {
public:
    App() = default;
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    // Initialize SDL, the window, renderer, and font subsystem. Returns false
    // (after logging) if anything fails.
    bool init();

    // Run the loop until the scene stack empties or the window is closed.
    void run();

    SceneManager& scenes() { return scenes_; }

private:
    void processEvents();
    void dispatchPointer(const SDL_Event& converted);

    WindowPtr window_;
    RendererPtr renderer_;
    std::unique_ptr<FontManager> fonts_;
    std::unique_ptr<Canvas> canvas_;
    SceneManager scenes_;
    bool ttfReady_ = false;
    bool running_ = false;
};

} // namespace og
