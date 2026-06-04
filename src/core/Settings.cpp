#include "core/Settings.hpp"

#include "core/Sdl.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <system_error>

namespace og {
namespace {

constexpr const char* kPrefOrg = "offline-games";
constexpr const char* kPrefApp = "offline-games";
constexpr const char* kFileName = "settings.cfg";

// Snap an arbitrary fps value to the nearest supported stop (ties favour the
// lower stop, since the loop only replaces on a strictly smaller distance).
[[nodiscard]] int snapFps(int fps) {
    int best = kFpsStops.front();
    int bestDist = std::abs(fps - best);
    for (const int stop : kFpsStops) {
        const int dist = std::abs(fps - stop);
        if (dist < bestDist) {
            best = stop;
            bestDist = dist;
        }
    }
    return best;
}

// Drop surrounding spaces/tabs and a trailing '\r' so CRLF files parse cleanly.
[[nodiscard]] std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

// std::from_chars is locale-independent (always '.'), unlike std::stof/scanf, so
// the file reads the same on every machine. On a bad value `out` is left alone.
bool parseInt(std::string_view v, int& out) {
    const auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
    return ec == std::errc{} && ptr == v.data() + v.size();
}

bool parseFloat(std::string_view v, float& out) {
    const auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
    return ec == std::errc{} && ptr == v.data() + v.size();
}

void parseBool(std::string_view v, bool& out) {
    int value = 0;
    if (parseInt(v, value)) {
        out = value != 0;
    }
}

[[nodiscard]] std::string prefFilePath() {
    char* prefRaw = SDL_GetPrefPath(kPrefOrg, kPrefApp);
    if (prefRaw == nullptr) {
        return {};
    }
    const SdlCharPtr pref(prefRaw);
    return std::string(pref.get()) + kFileName;
}

} // namespace

std::string serialize(const Settings& settings) {
    std::array<char, 16> volume{};
    std::snprintf(volume.data(), volume.size(), "%.2f", static_cast<double>(settings.volume));
    std::string out;
    out += "darkMode=";
    out += settings.darkMode ? "1" : "0";
    out += "\nvolume=";
    out += volume.data();
    out += "\nmusic=";
    out += settings.music ? "1" : "0";
    out += "\nvibration=";
    out += settings.vibration ? "1" : "0";
    out += "\nmaxFps=";
    out += std::to_string(settings.maxFps);
    out += "\ntapmatchLevelEasy=";
    out += std::to_string(settings.tapmatchLevelEasy);
    out += "\ntapmatchLevelMedium=";
    out += std::to_string(settings.tapmatchLevelMedium);
    out += "\ntapmatchLevelHard=";
    out += std::to_string(settings.tapmatchLevelHard);
    out += "\nblockfillLevelEasy=";
    out += std::to_string(settings.blockfillLevelEasy);
    out += "\nblockfillLevelMedium=";
    out += std::to_string(settings.blockfillLevelMedium);
    out += "\nblockfillLevelHard=";
    out += std::to_string(settings.blockfillLevelHard);
    out += "\nblockfillLevelVeryHard=";
    out += std::to_string(settings.blockfillLevelVeryHard);
    out += "\nminesweeperStreakEasy=";
    out += std::to_string(settings.minesweeperStreakEasy);
    out += "\nminesweeperStreakMedium=";
    out += std::to_string(settings.minesweeperStreakMedium);
    out += "\nminesweeperStreakHard=";
    out += std::to_string(settings.minesweeperStreakHard);
    out += "\nminesweeperBestEasy=";
    out += std::to_string(settings.minesweeperBestEasy);
    out += "\nminesweeperBestMedium=";
    out += std::to_string(settings.minesweeperBestMedium);
    out += "\nminesweeperBestHard=";
    out += std::to_string(settings.minesweeperBestHard);
    out += "\nsnakeBestEasy=";
    out += std::to_string(settings.snakeBestEasy);
    out += "\nsnakeBestMedium=";
    out += std::to_string(settings.snakeBestMedium);
    out += "\nsnakeBestHard=";
    out += std::to_string(settings.snakeBestHard);
    out += "\nhexanautBestEasy=";
    out += std::to_string(settings.hexanautBestEasy);
    out += "\nhexanautBestMedium=";
    out += std::to_string(settings.hexanautBestMedium);
    out += "\nhexanautBestHard=";
    out += std::to_string(settings.hexanautBestHard);
    out += "\n";
    return out;
}

Settings parse(std::string_view text) {
    Settings settings; // start from defaults; only known, well-formed lines override
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t nl = text.find('\n', pos);
        const std::size_t lineLen = (nl == std::string_view::npos) ? text.size() - pos : nl - pos;
        const std::string_view line = trim(text.substr(pos, lineLen));
        pos = (nl == std::string_view::npos) ? text.size() : nl + 1;

        const std::size_t eq = line.find('=');
        if (line.empty() || eq == std::string_view::npos || eq == 0) {
            continue; // blank line, no '=', or empty key
        }
        const std::string_view key = trim(line.substr(0, eq));
        const std::string_view value = trim(line.substr(eq + 1));
        if (key == "darkMode") {
            parseBool(value, settings.darkMode);
        } else if (key == "volume") {
            parseFloat(value, settings.volume);
        } else if (key == "music") {
            parseBool(value, settings.music);
        } else if (key == "vibration") {
            parseBool(value, settings.vibration);
        } else if (key == "maxFps") {
            parseInt(value, settings.maxFps);
        } else if (key == "tapmatchLevelEasy") {
            parseInt(value, settings.tapmatchLevelEasy);
        } else if (key == "tapmatchLevelMedium") {
            parseInt(value, settings.tapmatchLevelMedium);
        } else if (key == "tapmatchLevelHard") {
            parseInt(value, settings.tapmatchLevelHard);
        } else if (key == "blockfillLevelEasy") {
            parseInt(value, settings.blockfillLevelEasy);
        } else if (key == "blockfillLevelMedium") {
            parseInt(value, settings.blockfillLevelMedium);
        } else if (key == "blockfillLevelHard") {
            parseInt(value, settings.blockfillLevelHard);
        } else if (key == "blockfillLevelVeryHard") {
            parseInt(value, settings.blockfillLevelVeryHard);
        } else if (key == "minesweeperStreakEasy") {
            parseInt(value, settings.minesweeperStreakEasy);
        } else if (key == "minesweeperStreakMedium") {
            parseInt(value, settings.minesweeperStreakMedium);
        } else if (key == "minesweeperStreakHard") {
            parseInt(value, settings.minesweeperStreakHard);
        } else if (key == "minesweeperBestEasy") {
            parseInt(value, settings.minesweeperBestEasy);
        } else if (key == "minesweeperBestMedium") {
            parseInt(value, settings.minesweeperBestMedium);
        } else if (key == "minesweeperBestHard") {
            parseInt(value, settings.minesweeperBestHard);
        } else if (key == "snakeBestEasy") {
            parseInt(value, settings.snakeBestEasy);
        } else if (key == "snakeBestMedium") {
            parseInt(value, settings.snakeBestMedium);
        } else if (key == "snakeBestHard") {
            parseInt(value, settings.snakeBestHard);
        } else if (key == "hexanautBestEasy") {
            parseInt(value, settings.hexanautBestEasy);
        } else if (key == "hexanautBestMedium") {
            parseInt(value, settings.hexanautBestMedium);
        } else if (key == "hexanautBestHard") {
            parseInt(value, settings.hexanautBestHard);
        }
        // Unknown keys are ignored so older/newer files stay forward-compatible.
    }
    settings.volume = std::clamp(settings.volume, 0.0F, 1.0F);
    settings.maxFps = snapFps(settings.maxFps);
    settings.tapmatchLevelEasy = std::max(1, settings.tapmatchLevelEasy);
    settings.tapmatchLevelMedium = std::max(1, settings.tapmatchLevelMedium);
    settings.tapmatchLevelHard = std::max(1, settings.tapmatchLevelHard);
    settings.blockfillLevelEasy = std::max(1, settings.blockfillLevelEasy);
    settings.blockfillLevelMedium = std::max(1, settings.blockfillLevelMedium);
    settings.blockfillLevelHard = std::max(1, settings.blockfillLevelHard);
    settings.blockfillLevelVeryHard = std::max(1, settings.blockfillLevelVeryHard);
    settings.minesweeperStreakEasy = std::max(0, settings.minesweeperStreakEasy);
    settings.minesweeperStreakMedium = std::max(0, settings.minesweeperStreakMedium);
    settings.minesweeperStreakHard = std::max(0, settings.minesweeperStreakHard);
    settings.minesweeperBestEasy = std::max(0, settings.minesweeperBestEasy);
    settings.minesweeperBestMedium = std::max(0, settings.minesweeperBestMedium);
    settings.minesweeperBestHard = std::max(0, settings.minesweeperBestHard);
    settings.snakeBestEasy = std::max(0, settings.snakeBestEasy);
    settings.snakeBestMedium = std::max(0, settings.snakeBestMedium);
    settings.snakeBestHard = std::max(0, settings.snakeBestHard);
    settings.hexanautBestEasy = std::max(0, settings.hexanautBestEasy);
    settings.hexanautBestMedium = std::max(0, settings.hexanautBestMedium);
    settings.hexanautBestHard = std::max(0, settings.hexanautBestHard);
    return settings;
}

Settings loadSettings() {
    const std::string path = prefFilePath();
    if (path.empty()) {
        return Settings{};
    }
    std::size_t size = 0;
    void* raw = SDL_LoadFile(path.c_str(), &size);
    if (raw == nullptr) {
        return Settings{}; // missing or unreadable file → defaults
    }
    const SdlCharPtr data(static_cast<char*>(raw));
    return parse(std::string_view(data.get(), size));
}

void saveSettings(const Settings& settings) {
    const std::string path = prefFilePath();
    if (path.empty()) {
        return;
    }
    const std::string text = serialize(settings);
    if (!SDL_SaveFile(path.c_str(), text.data(), text.size())) {
        SDL_Log("Failed to save settings to %s: %s", path.c_str(), SDL_GetError());
    }
}

Settings& settings() {
    static Settings instance = loadSettings();
    return instance;
}

} // namespace og
