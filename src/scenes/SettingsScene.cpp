#include "scenes/SettingsScene.hpp"

#include "core/Canvas.hpp"
#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/SceneManager.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace og {
namespace {

// ---- Back button (circular, top-left) — matches the other scenes -----------
constexpr float kBackCx = 92.0F;
constexpr float kBackCy = 100.0F;
constexpr float kBackRadius = 56.0F;

// ---- Shared row geometry --------------------------------------------------
constexpr float kLabelX = 160.0F;               // left-aligned label x for every row
constexpr float kLabelSize = 40.0F;             // label text height
constexpr float kTrackX = 160.0F;               // slider track left
constexpr float kTrackW = 400.0F;               // slider track width (ends at 560)
constexpr float kRightEdge = kTrackX + kTrackW; // 560: toggles right-align here
constexpr float kToggleW = 116.0F;
constexpr float kToggleH = 60.0F;
constexpr float kToggleX = kRightEdge - kToggleW;

// Toggle rows: label vertically centered with the toggle.
constexpr float kDarkRowCy = 320.0F;
constexpr float kMusicRowCy = 820.0F;
constexpr float kVibrationRowCy = 940.0F;

// Slider rows: label above, track below.
constexpr float kFpsLabelCy = 470.0F;
constexpr float kFpsTrackCy = 542.0F;
constexpr float kVolumeLabelCy = 650.0F;
constexpr float kVolumeTrackCy = 722.0F;

float toggleY(float rowCy) {
    return rowCy - (kToggleH / 2.0F);
}

void drawLabel(Canvas& canvas, const char* text, float centerY) {
    canvas.text(text, kLabelX, centerY - (kLabelSize / 2.0F), kLabelSize, theme().bodyText,
                Canvas::Align::Left);
}

} // namespace

SettingsScene::SettingsScene(SceneManager& manager)
    : manager_(manager), darkToggle_(kToggleX, toggleY(kDarkRowCy), kToggleW, kToggleH),
      fpsSlider_(kTrackX, kFpsTrackCy, kTrackW, static_cast<float>(kFpsStops.front()),
                 static_cast<float>(kFpsStops.back())),
      volumeSlider_(kTrackX, kVolumeTrackCy, kTrackW, 0.0F, 1.0F),
      musicToggle_(kToggleX, toggleY(kMusicRowCy), kToggleW, kToggleH),
      vibrationToggle_(kToggleX, toggleY(kVibrationRowCy), kToggleW, kToggleH) {
    darkToggle_.setValue(settings().darkMode);
    darkToggle_.setOnChange([this](bool on) {
        settings().darkMode = on;
        dirty_ = true;
    });

    std::vector<float> fpsStops;
    fpsStops.reserve(kFpsStops.size());
    for (const int fps : kFpsStops) {
        fpsStops.push_back(static_cast<float>(fps));
    }
    fpsSlider_.setStops(std::move(fpsStops));
    fpsSlider_.setValue(static_cast<float>(settings().maxFps));
    fpsSlider_.setOnChange([this](float v) {
        settings().maxFps = static_cast<int>(std::lround(v));
        dirty_ = true;
    });

    volumeSlider_.setValue(settings().volume);
    volumeSlider_.setOnChange([this](float v) {
        settings().volume = v;
        dirty_ = true;
    });

    musicToggle_.setValue(settings().music);
    musicToggle_.setOnChange([this](bool on) {
        settings().music = on;
        dirty_ = true;
    });

    vibrationToggle_.setValue(settings().vibration);
    vibrationToggle_.setOnChange([this](bool on) {
        settings().vibration = on;
        dirty_ = true;
    });
}

bool SettingsScene::handleBackButton(const PointerEvent& event) {
    if (event.phase == PointerEvent::Phase::Move) {
        return false;
    }
    const bool inside = hitTest(event, kBackCx - kBackRadius, kBackCy - kBackRadius,
                                kBackRadius * 2.0F, kBackRadius * 2.0F);
    if (event.phase == PointerEvent::Phase::Down) {
        backPressed_ = inside;
        return inside;
    }
    const bool wasPressed = backPressed_;
    backPressed_ = false;
    if (wasPressed && inside) {
        manager_.pop();
        return true;
    }
    return false;
}

void SettingsScene::handleInput(const PointerEvent& event) {
    if (handleBackButton(event)) {
        return;
    }
    // The controls don't overlap, so dispatching to each is harmless; each only
    // consumes events inside its own bounds. A change marks the scene dirty.
    darkToggle_.handleInput(event);
    fpsSlider_.handleInput(event);
    volumeSlider_.handleInput(event);
    musicToggle_.handleInput(event);
    vibrationToggle_.handleInput(event);

    // Persist once per interaction, on release, rather than on every drag frame.
    if (event.phase == PointerEvent::Phase::Up && dirty_) {
        saveSettings(settings());
        dirty_ = false;
    }
}

void SettingsScene::update(float /*dtSeconds*/) {}

void SettingsScene::render(Canvas& canvas) {
    canvas.clear(theme().menuBg);

    // Back button.
    canvas.fillCircle(kBackCx, kBackCy, kBackRadius, theme().backCircle);
    canvas.line(kBackCx + 12.0F, kBackCy - 24.0F, kBackCx - 14.0F, kBackCy, 14.0F, theme().chevron);
    canvas.line(kBackCx - 14.0F, kBackCy, kBackCx + 12.0F, kBackCy + 24.0F, 14.0F, theme().chevron);

    canvas.textCentered("SETTINGS", layout::kWidthF / 2.0F, 150.0F, 64.0F, theme().titleText);

    drawLabel(canvas, "Dark Mode", kDarkRowCy);
    drawLabel(canvas, "Maximum FPS", kFpsLabelCy);
    drawLabel(canvas, "Volume", kVolumeLabelCy);
    drawLabel(canvas, "Music", kMusicRowCy);
    drawLabel(canvas, "Vibration", kVibrationRowCy);

    // Current FPS readout, right-aligned above the FPS slider's right end.
    canvas.text(std::to_string(settings().maxFps), kRightEdge, kFpsLabelCy - (kLabelSize / 2.0F),
                kLabelSize, colors::menuPurple, Canvas::Align::Right);

    darkToggle_.render(canvas);
    fpsSlider_.render(canvas);
    volumeSlider_.render(canvas);
    musicToggle_.render(canvas);
    vibrationToggle_.render(canvas);
}

} // namespace og
