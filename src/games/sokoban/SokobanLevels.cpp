#include "games/sokoban/SokobanLevels.hpp"

#include "core/Sdl.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <SDL3/SDL.h>
#include <span>
#include <string>

namespace og {
namespace {

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
[[nodiscard]] bool readU8(std::span<const std::uint8_t> b, std::size_t pos, std::uint8_t& out) {
    if (pos >= b.size()) {
        return false;
    }
    out = b[pos];
    return true;
}

[[nodiscard]] bool readU16(std::span<const std::uint8_t> b, std::size_t pos, std::uint16_t& out) {
    if (pos + 2 > b.size()) {
        return false;
    }
    out = static_cast<std::uint16_t>(static_cast<unsigned>(b[pos]) |
                                     (static_cast<unsigned>(b[pos + 1]) << 8));
    return true;
}

[[nodiscard]] bool readU32(std::span<const std::uint8_t> b, std::size_t pos, std::uint32_t& out) {
    if (pos + 4 > b.size()) {
        return false;
    }
    out = static_cast<std::uint32_t>(b[pos]) | (static_cast<std::uint32_t>(b[pos + 1]) << 8) |
          (static_cast<std::uint32_t>(b[pos + 2]) << 16) |
          (static_cast<std::uint32_t>(b[pos + 3]) << 24);
    return true;
}
// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

constexpr std::uint32_t kMagic = 0x4F4B4F53; // "SOKO" little-endian
constexpr const char* kLevelsAsset = "assets/sokoban/levels.bytes";
constexpr std::array<std::string_view, 10> kSetNames{
    "Sasquatch",    "Mas Sasquatch", "Sasquatch III",       "Microban (easy)",
    "Sasquatch IV", "Sasquatch V",   "Mas Microban (easy)", "Sasquatch VI",
    "LOMA",         "Sasquatch VII",
};

[[nodiscard]] bool readRecord(std::span<const std::uint8_t> b, std::size_t& pos,
                              SokobanLevel& out) {
    std::uint8_t width = 0;
    std::uint8_t height = 0;
    std::uint8_t sourceSet = 0;
    std::uint16_t sourceLevel = 0;
    if (!readU8(b, pos, width) || !readU8(b, pos + 1, height) || !readU8(b, pos + 2, sourceSet) ||
        !readU16(b, pos + 3, sourceLevel)) {
        return false;
    }
    pos += 5;
    if (width == 0 || height == 0 || sourceLevel == 0) {
        return false;
    }
    const std::size_t cells = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (pos + cells > b.size()) {
        return false;
    }

    out.width = width;
    out.height = height;
    out.sourceSet = sourceSet;
    out.sourceLevel = sourceLevel;
    out.cells.clear();
    out.cells.reserve(cells);
    for (std::size_t i = 0; i < cells; ++i) {
        std::uint8_t cell = 0;
        if (!readU8(b, pos + i, cell)) {
            return false;
        }
        out.cells.push_back(static_cast<char>(cell));
    }
    pos += cells;
    return true;
}

[[nodiscard]] std::string bundledLevelsPath() {
    const char* base = SDL_GetBasePath();
    if (base == nullptr) {
        return {};
    }
    return std::string(base) + kLevelsAsset;
}

} // namespace

int SokobanLevel::boxCount() const {
    return static_cast<int>(
        std::ranges::count_if(cells, [](char cell) { return cell == '$' || cell == '*'; }));
}

int SokobanLevel::goalCount() const {
    return static_cast<int>(std::ranges::count_if(
        cells, [](char cell) { return cell == '.' || cell == '*' || cell == '+'; }));
}

std::array<std::vector<SokobanLevel>, kSokobanTierCount>
parseSokobanLevels(std::span<const std::uint8_t> bytes) {
    std::array<std::vector<SokobanLevel>, kSokobanTierCount> tiers;

    std::uint32_t magic = 0;
    std::uint16_t version = 0;
    std::uint16_t tierCount = 0;
    if (!readU32(bytes, 0, magic) || magic != kMagic || !readU16(bytes, 4, version) ||
        version != 1 || !readU16(bytes, 6, tierCount) || tierCount != kSokobanTierCount) {
        return tiers;
    }

    std::array<std::uint16_t, kSokobanTierCount> counts{};
    for (int t = 0; t < kSokobanTierCount; ++t) {
        if (!readU16(bytes, 8 + (static_cast<std::size_t>(t) * 2),
                     counts.at(static_cast<std::size_t>(t)))) {
            return tiers;
        }
    }

    std::size_t pos = 8 + (static_cast<std::size_t>(kSokobanTierCount) * 2);
    for (int t = 0; t < kSokobanTierCount; ++t) {
        const std::uint16_t want = counts.at(static_cast<std::size_t>(t));
        tiers.at(static_cast<std::size_t>(t)).reserve(want);
        for (std::uint16_t i = 0; i < want; ++i) {
            SokobanLevel level;
            if (!readRecord(bytes, pos, level)) {
                return tiers;
            }
            tiers.at(static_cast<std::size_t>(t)).push_back(std::move(level));
        }
    }
    return tiers;
}

const std::array<std::vector<SokobanLevel>, kSokobanTierCount>& sokobanLevels() {
    static const std::array<std::vector<SokobanLevel>, kSokobanTierCount> catalog = [] {
        const std::string path = bundledLevelsPath();
        if (path.empty()) {
            SDL_Log("Sokoban: could not resolve base path for %s", kLevelsAsset);
            return std::array<std::vector<SokobanLevel>, kSokobanTierCount>{};
        }
        std::size_t size = 0;
        void* raw = SDL_LoadFile(path.c_str(), &size);
        if (raw == nullptr) {
            SDL_Log("Sokoban: failed to load %s: %s", path.c_str(), SDL_GetError());
            return std::array<std::vector<SokobanLevel>, kSokobanTierCount>{};
        }
        const SdlCharPtr data(static_cast<char*>(raw));
        const auto* first = static_cast<const std::uint8_t*>(raw);
        return parseSokobanLevels(std::span<const std::uint8_t>(first, size));
    }();
    return catalog;
}

int sokobanTierSize(int tier) {
    if (tier < 0 || tier >= kSokobanTierCount) {
        return 0;
    }
    return static_cast<int>(sokobanLevels().at(static_cast<std::size_t>(tier)).size());
}

const SokobanLevel* sokobanTierLevel(int tier, int level) {
    if (tier < 0 || tier >= kSokobanTierCount) {
        return nullptr;
    }
    const std::vector<SokobanLevel>& pool = sokobanLevels().at(static_cast<std::size_t>(tier));
    if (pool.empty()) {
        return nullptr;
    }
    const int clamped = std::clamp(level, 1, static_cast<int>(pool.size()));
    return &pool.at(static_cast<std::size_t>(clamped - 1));
}

std::string_view sokobanSetName(int sourceSet) {
    if (sourceSet < 0) {
        return "Unknown";
    }
    const auto index = static_cast<std::size_t>(sourceSet);
    if (index >= kSetNames.size()) {
        return "Unknown";
    }
    return kSetNames.at(index);
}

} // namespace og
