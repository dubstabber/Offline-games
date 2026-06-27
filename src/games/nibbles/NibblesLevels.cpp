#include "games/nibbles/NibblesLevels.hpp"

#include "core/Sdl.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <SDL3/SDL.h>
#include <span>
#include <string>
#include <vector>

namespace og::nibbles {
namespace {

constexpr std::uint32_t kMagic = 0x4242494E; // "NIBB" little-endian
constexpr const char* kLevelsAsset = "assets/nibbles/levels.bytes";

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

[[nodiscard]] Direction directionFromByte(std::uint8_t value) {
    switch (value) {
    case 0:
        return Direction::Up;
    case 1:
        return Direction::Right;
    case 2:
        return Direction::Down;
    case 3:
        return Direction::Left;
    default:
        break;
    }
    return Direction::Right;
}

[[nodiscard]] bool inBounds(const NibblesLevel& level, Position pos) {
    return pos.x >= 0 && pos.y >= 0 && pos.x < level.width && pos.y < level.height;
}

[[nodiscard]] bool readRecord(std::span<const std::uint8_t> b, std::size_t& pos,
                              NibblesLevel& out) {
    std::uint8_t width = 0;
    std::uint8_t height = 0;
    std::uint8_t sourceLevel = 0;
    std::uint8_t spawnCount = 0;
    std::uint8_t warpCount = 0;
    if (!readU8(b, pos, width) || !readU8(b, pos + 1, height) || !readU8(b, pos + 2, sourceLevel) ||
        !readU8(b, pos + 3, spawnCount) || !readU8(b, pos + 4, warpCount)) {
        return false;
    }
    pos += 5;
    if (width == 0 || height == 0 || sourceLevel == 0 || spawnCount == 0) {
        return false;
    }

    const std::size_t cells = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (pos + cells > b.size()) {
        return false;
    }

    out = {};
    out.width = width;
    out.height = height;
    out.sourceLevel = sourceLevel;
    out.cells.reserve(cells);
    for (std::size_t i = 0; i < cells; ++i) {
        std::uint8_t cell = 0;
        if (!readU8(b, pos + i, cell) || cell > static_cast<std::uint8_t>(Cell::Warp)) {
            return false;
        }
        out.cells.push_back(static_cast<Cell>(cell));
    }
    pos += cells;

    out.spawns.reserve(spawnCount);
    for (std::uint8_t i = 0; i < spawnCount; ++i) {
        std::uint8_t x = 0;
        std::uint8_t y = 0;
        std::uint8_t direction = 0;
        if (!readU8(b, pos, x) || !readU8(b, pos + 1, y) || !readU8(b, pos + 2, direction) ||
            direction > 3) {
            return false;
        }
        pos += 3;
        const Spawn spawn{.pos = {.x = static_cast<int>(x), .y = static_cast<int>(y)},
                          .direction = directionFromByte(direction)};
        if (!inBounds(out, spawn.pos)) {
            return false;
        }
        out.spawns.push_back(spawn);
    }

    out.warps.reserve(warpCount);
    for (std::uint8_t i = 0; i < warpCount; ++i) {
        std::uint8_t id = 0;
        std::uint8_t sx = 0;
        std::uint8_t sy = 0;
        std::uint8_t tx = 0;
        std::uint8_t ty = 0;
        std::uint8_t flags = 0;
        if (!readU8(b, pos, id) || !readU8(b, pos + 1, sx) || !readU8(b, pos + 2, sy) ||
            !readU8(b, pos + 3, tx) || !readU8(b, pos + 4, ty) || !readU8(b, pos + 5, flags) ||
            (flags & static_cast<std::uint8_t>(~3U)) != 0U) {
            return false;
        }
        pos += 6;
        Warp warp{.id = static_cast<int>(id),
                  .source = {.x = static_cast<int>(sx), .y = static_cast<int>(sy)},
                  .target = {.x = static_cast<int>(tx), .y = static_cast<int>(ty)},
                  .random = (flags & 1U) != 0U,
                  .bidirectional = (flags & 2U) != 0U};
        if (!inBounds(out, warp.source) ||
            !inBounds(out, {.x = warp.source.x + 1, .y = warp.source.y + 1}) ||
            (!warp.random && !inBounds(out, warp.target))) {
            return false;
        }
        out.warps.push_back(warp);
    }

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

std::vector<NibblesLevel> parseNibblesLevels(std::span<const std::uint8_t> bytes) {
    std::uint32_t magic = 0;
    std::uint16_t version = 0;
    std::uint16_t count = 0;
    if (!readU32(bytes, 0, magic) || magic != kMagic || !readU16(bytes, 4, version) ||
        version != 1 || !readU16(bytes, 6, count)) {
        return {};
    }

    std::vector<NibblesLevel> levels;
    levels.reserve(count);
    std::size_t pos = 8;
    for (std::uint16_t i = 0; i < count; ++i) {
        NibblesLevel level;
        if (!readRecord(bytes, pos, level)) {
            return {};
        }
        levels.push_back(std::move(level));
    }
    if (pos != bytes.size()) {
        return {};
    }
    return levels;
}

const std::vector<NibblesLevel>& nibblesLevels() {
    static const std::vector<NibblesLevel> catalog = [] {
        const std::string path = bundledLevelsPath();
        if (path.empty()) {
            SDL_Log("Nibbles: could not resolve base path for %s", kLevelsAsset);
            return std::vector<NibblesLevel>{};
        }
        std::size_t size = 0;
        void* raw = SDL_LoadFile(path.c_str(), &size);
        if (raw == nullptr) {
            SDL_Log("Nibbles: failed to load %s: %s", path.c_str(), SDL_GetError());
            return std::vector<NibblesLevel>{};
        }
        const SdlCharPtr data(static_cast<char*>(raw));
        const auto* first = static_cast<const std::uint8_t*>(raw);
        return parseNibblesLevels(std::span<const std::uint8_t>(first, size));
    }();
    return catalog;
}

int nibblesLevelCount() {
    return static_cast<int>(nibblesLevels().size());
}

const NibblesLevel* nibblesLevel(int level) {
    const std::vector<NibblesLevel>& levels = nibblesLevels();
    if (levels.empty()) {
        return nullptr;
    }
    const int clamped = std::clamp(level, 1, static_cast<int>(levels.size()));
    return &levels.at(static_cast<std::size_t>(clamped - 1));
}

} // namespace og::nibbles
