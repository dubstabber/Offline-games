#pragma once

namespace og {

class Canvas;
struct PointerEvent;
class SceneManager;

// Base class for everything on screen: the menu and every game are Scenes.
// The lifecycle each frame is handleInput() -> update(dt) -> render(). A scene
// drives navigation through the SceneManager it is given on construction
// (push a game, pop back to the menu, etc.).
class Scene {
public:
    Scene() = default;
    virtual ~Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;

    // One discrete pointer interaction, already in logical canvas coordinates.
    virtual void handleInput(const PointerEvent& event) = 0;

    // Advance animation/state. dtSeconds is the wall-clock delta for this frame.
    virtual void update(float dtSeconds) = 0;

    // Draw the scene. The canvas is already cleared to the background color.
    virtual void render(Canvas& canvas) = 0;

    // Whether the scene needs to be redrawn continuously. Games animate (the
    // default); the static chrome scenes override to false so the App loop can
    // idle — skipping the draw and present when nothing changed (no input, no
    // resize, no scene switch) — to save the PinePhone's battery. A scene that is
    // only sometimes static (the menu, whose fling scroll settles to rest) may
    // return a value that varies frame to frame.
    [[nodiscard]] virtual bool isAnimating() const { return true; }
};

} // namespace og
