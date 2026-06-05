#pragma once

#include "core/Scene.hpp"
#include "ui/IconButton.hpp"
#include "ui/Slider.hpp"
#include "ui/Toggle.hpp"

namespace og {

class SceneManager;

// The options screen, opened from the menu's hamburger button. A back button, a
// "SETTINGS" title, and a column of labeled controls: Dark Mode (toggle, live),
// Maximum FPS (slider, live), Volume (slider), Music and Vibration (toggles).
// Every change writes straight into the global settings() and is persisted to
// disk on pointer release. Dark Mode recolors the whole app immediately because
// scenes read theme() each frame.
class SettingsScene : public Scene {
public:
    explicit SettingsScene(SceneManager& manager);

    void handleInput(const PointerEvent& event) override;
    void update(float dtSeconds) override;
    void render(Canvas& canvas) override;
    // Static screen: only redraws on input, so the App loop idles otherwise.
    [[nodiscard]] bool isAnimating() const override { return false; }

private:
    SceneManager& manager_;
    Toggle darkToggle_;
    Slider fpsSlider_;
    Slider volumeSlider_;
    Toggle musicToggle_;
    Toggle vibrationToggle_;
    IconButton backButton_;
    bool dirty_ = false; // a control changed since the last save
};

} // namespace og
