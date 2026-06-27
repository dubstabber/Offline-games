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

// Field descriptor tables: the single source of truth for every persisted
// setting, so serialize(), parse() and the clamp pass iterate one list instead
// of repeating the ~22 fields three times. maxFps is intentionally absent — it
// snaps to kFpsStops rather than flooring, so it stays handled by name.
struct BoolField {
    const char* key;
    bool Settings::* member;
};
struct FloatField {
    const char* key;
    float Settings::* member;
};
struct IntField {
    const char* key;
    int Settings::* member;
    int clampMin;
};

constexpr std::array<BoolField, 3> kBoolFields{{
    {.key = "darkMode", .member = &Settings::darkMode},
    {.key = "music", .member = &Settings::music},
    {.key = "vibration", .member = &Settings::vibration},
}};

constexpr std::array<FloatField, 1> kFloatFields{{
    {.key = "volume", .member = &Settings::volume},
}};

// Order here is also the on-disk serialize order, so it must not change without
// matching serialize(). clampMin floors each field (levels at 1, scores at 0).
constexpr std::array<IntField, 28> kIntFields{{
    {.key = "tapmatchLevelEasy", .member = &Settings::tapmatchLevelEasy, .clampMin = 1},
    {.key = "tapmatchLevelMedium", .member = &Settings::tapmatchLevelMedium, .clampMin = 1},
    {.key = "tapmatchLevelHard", .member = &Settings::tapmatchLevelHard, .clampMin = 1},
    {.key = "blockfillLevelEasy", .member = &Settings::blockfillLevelEasy, .clampMin = 1},
    {.key = "blockfillLevelMedium", .member = &Settings::blockfillLevelMedium, .clampMin = 1},
    {.key = "blockfillLevelHard", .member = &Settings::blockfillLevelHard, .clampMin = 1},
    {.key = "blockfillLevelVeryHard", .member = &Settings::blockfillLevelVeryHard, .clampMin = 1},
    {.key = "sokobanLevelEasy", .member = &Settings::sokobanLevelEasy, .clampMin = 1},
    {.key = "sokobanLevelMedium", .member = &Settings::sokobanLevelMedium, .clampMin = 1},
    {.key = "sokobanLevelHard", .member = &Settings::sokobanLevelHard, .clampMin = 1},
    {.key = "nibblesLevelEasy", .member = &Settings::nibblesLevelEasy, .clampMin = 1},
    {.key = "nibblesLevelMedium", .member = &Settings::nibblesLevelMedium, .clampMin = 1},
    {.key = "nibblesLevelHard", .member = &Settings::nibblesLevelHard, .clampMin = 1},
    {.key = "minesweeperStreakEasy", .member = &Settings::minesweeperStreakEasy, .clampMin = 0},
    {.key = "minesweeperStreakMedium", .member = &Settings::minesweeperStreakMedium, .clampMin = 0},
    {.key = "minesweeperStreakHard", .member = &Settings::minesweeperStreakHard, .clampMin = 0},
    {.key = "minesweeperBestEasy", .member = &Settings::minesweeperBestEasy, .clampMin = 0},
    {.key = "minesweeperBestMedium", .member = &Settings::minesweeperBestMedium, .clampMin = 0},
    {.key = "minesweeperBestHard", .member = &Settings::minesweeperBestHard, .clampMin = 0},
    {.key = "snakeBestEasy", .member = &Settings::snakeBestEasy, .clampMin = 0},
    {.key = "snakeBestMedium", .member = &Settings::snakeBestMedium, .clampMin = 0},
    {.key = "snakeBestHard", .member = &Settings::snakeBestHard, .clampMin = 0},
    {.key = "hexanautBestEasy", .member = &Settings::hexanautBestEasy, .clampMin = 0},
    {.key = "hexanautBestMedium", .member = &Settings::hexanautBestMedium, .clampMin = 0},
    {.key = "hexanautBestHard", .member = &Settings::hexanautBestHard, .clampMin = 0},
    {.key = "holeBestEasy", .member = &Settings::holeBestEasy, .clampMin = 0},
    {.key = "holeBestMedium", .member = &Settings::holeBestMedium, .clampMin = 0},
    {.key = "holeBestHard", .member = &Settings::holeBestHard, .clampMin = 0},
}};

// Apply one parsed key=value line to the matching field. Unknown keys are
// ignored so older/newer files stay forward-compatible.
void applyKv(Settings& settings, std::string_view key, std::string_view value) {
    if (key == "maxFps") {
        parseInt(value, settings.maxFps);
        return;
    }
    for (const BoolField& field : kBoolFields) {
        if (key == field.key) {
            parseBool(value, settings.*field.member);
            return;
        }
    }
    for (const FloatField& field : kFloatFields) {
        if (key == field.key) {
            parseFloat(value, settings.*field.member);
            return;
        }
    }
    for (const IntField& field : kIntFields) {
        if (key == field.key) {
            parseInt(value, settings.*field.member);
            return;
        }
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
    // The per-game ints follow in table order, matching the layout above.
    for (const IntField& field : kIntFields) {
        out += '\n';
        out += field.key;
        out += '=';
        out += std::to_string(settings.*field.member);
    }
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
        applyKv(settings, key, value);
    }
    settings.volume = std::clamp(settings.volume, 0.0F, 1.0F);
    settings.maxFps = snapFps(settings.maxFps);
    for (const IntField& field : kIntFields) {
        settings.*field.member = std::max(field.clampMin, settings.*field.member);
    }
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
