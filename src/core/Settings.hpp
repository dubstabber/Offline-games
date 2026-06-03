#pragma once

#include <array>
#include <string>
#include <string_view>

namespace og {

// The supported Maximum-FPS slider stops. parse() snaps any stored value to the
// nearest of these, and the FPS slider in the settings screen uses them too.
inline constexpr std::array<int, 4> kFpsStops{30, 60, 90, 120};

// Persistent user preferences. Only darkMode and maxFps currently affect the
// app; volume/music/vibration are stored for a future audio/haptics layer.
struct Settings {
    bool darkMode = false;
    float volume = 0.8F;
    bool music = true;
    bool vibration = true;
    int maxFps = 120;
    // Tap Match progress: the current level (1-based) reached in each difficulty.
    // Clearing the level advances its counter; the difficulty screen shows it on
    // PLAY. Each difficulty has its own pool of boards, so each tracks separately.
    int tapmatchLevelEasy = 1;
    int tapmatchLevelMedium = 1;
    int tapmatchLevelHard = 1;
};

// Pure and SDL-free (so they are unit-testable): turn Settings into the on-disk
// `key=value` text and back. parse() starts from defaults, ignores unknown,
// blank, and malformed lines, keeps a field's default on a bad value, and clamps
// every field — so its result is always a valid Settings.
[[nodiscard]] std::string serialize(const Settings& settings);
[[nodiscard]] Settings parse(std::string_view text);

// Read/write the settings file under SDL_GetPrefPath. loadSettings() returns
// defaults if the file is missing or unreadable; saveSettings() is best-effort
// and never throws or crashes on a write failure.
[[nodiscard]] Settings loadSettings();
void saveSettings(const Settings& settings);

// The one app-wide Settings instance, loaded from disk on first use. This mirrors
// the global `colors::` palette: theme/options are genuinely app-global state, so
// a global accessor is lower-churn than threading Settings through every scene.
// The first call must happen after SDL_Init (it reads SDL_GetPrefPath).
[[nodiscard]] Settings& settings();

} // namespace og
