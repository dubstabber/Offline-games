#include "games/tapmatch/TapMatchLevels.hpp"

#include "core/Sdl.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <SDL3/SDL.h>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace og {
namespace {

// Little-endian readers over the byte span. Each bounds-checks `pos` up front and
// returns false (leaving `out` alone) if too few bytes remain, so every operator[]
// below is provably in range. std::span has no .at() in C++20, hence the localised
// suppression rather than a checked accessor; all raw byte access goes through here.
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

constexpr const char* kLevelsAsset = "assets/tapmatch/levels.bytes";

// Path to the bundled level blob next to the executable (empty if SDL can't tell).
[[nodiscard]] std::string bundledLevelsPath() {
    const char* base = SDL_GetBasePath();
    if (base == nullptr) {
        return {};
    }
    return std::string(base) + kLevelsAsset;
}

} // namespace

std::vector<LevelLayout> parseTapMatchLevels(std::span<const std::uint8_t> bytes) {
    std::vector<LevelLayout> levels;

    std::uint32_t count = 0;
    if (!readU32(bytes, 0, count) || count == 0) {
        return levels;
    }
    // Each level's record offset is stored at byte 4*k (k is 1-indexed); the
    // records themselves live in a data section right after the offset table.
    const std::size_t dataBase = (static_cast<std::size_t>(count) * 4) + 4;
    levels.reserve(count);

    for (std::uint32_t k = 1; k <= count; ++k) {
        std::uint32_t recOffset = 0;
        if (!readU32(bytes, static_cast<std::size_t>(k) * 4, recOffset)) {
            break;
        }
        std::size_t pos = dataBase + recOffset;
        std::uint8_t header = 0;
        std::uint8_t tileCount = 0;
        if (!readU8(bytes, pos, header) || !readU8(bytes, pos + 1, tileCount)) {
            break; // need at least a header byte + a tile-count byte
        }
        pos += 2;

        LevelLayout level;
        level.iconsCount = header >> 3;
        level.initStackSize = ((header >> 1) & 0x3) + 5;
        level.tiles.reserve(tileCount);

        bool ok = true;
        for (std::uint8_t t = 0; t < tileCount; ++t) {
            std::uint16_t u = 0;
            if (!readU16(bytes, pos, u)) {
                ok = false;
                break;
            }
            pos += 2;
            const int layer = u >> 10;
            const int gx = (u >> 6) & 0xF;
            const int gy = (u >> 2) & 0xF;
            const int xDelta = (u >> 1) & 0x1;
            const int yDelta = u & 0x1;
            level.tiles.push_back(
                PlacedTile{.layer = layer, .x = (2 * gx) + xDelta, .y = (2 * gy) + yDelta});
        }
        if (!ok) {
            break; // truncated record: stop, but keep the fully-decoded levels
        }
        // Any FTUE (tutorial-hint) section that follows is intentionally ignored.
        levels.push_back(std::move(level));
    }
    return levels;
}

std::array<std::vector<int>, kTapMatchTierCount>
tapMatchTierPartition(const std::vector<LevelLayout>& levels) {
    std::vector<int> order(levels.size());
    for (std::size_t i = 0; i < levels.size(); ++i) {
        order.at(i) = static_cast<int>(i);
    }
    std::ranges::sort(order, [&levels](int a, int b) {
        return levels.at(static_cast<std::size_t>(a)).tiles.size() <
               levels.at(static_cast<std::size_t>(b)).tiles.size();
    });
    std::array<std::vector<int>, kTapMatchTierCount> out;
    const std::size_t n = std::max<std::size_t>(order.size(), 1);
    for (std::size_t rank = 0; rank < order.size(); ++rank) {
        // Split into near-equal contiguous bands by complexity rank.
        const auto tier =
            std::min<std::size_t>((rank * kTapMatchTierCount) / n, kTapMatchTierCount - 1);
        out.at(tier).push_back(order.at(rank));
    }
    return out;
}

namespace {

// Tiers over the bundled catalog, built once on first use.
[[nodiscard]] const std::array<std::vector<int>, kTapMatchTierCount>& tierIndices() {
    static const std::array<std::vector<int>, kTapMatchTierCount> tiers =
        tapMatchTierPartition(tapMatchLevels());
    return tiers;
}

} // namespace

int tapMatchTierSize(int tier) {
    if (tier < 0 || tier >= kTapMatchTierCount) {
        return 0;
    }
    return static_cast<int>(tierIndices().at(static_cast<std::size_t>(tier)).size());
}

const LevelLayout* tapMatchTierLevel(int tier, int level) {
    if (tier < 0 || tier >= kTapMatchTierCount) {
        return nullptr;
    }
    const std::vector<int>& idx = tierIndices().at(static_cast<std::size_t>(tier));
    if (idx.empty()) {
        return nullptr;
    }
    const int clamped = std::clamp(level, 1, static_cast<int>(idx.size()));
    const int which = idx.at(static_cast<std::size_t>(clamped - 1));
    return &tapMatchLevels().at(static_cast<std::size_t>(which));
}

const std::vector<LevelLayout>& tapMatchLevels() {
    static const std::vector<LevelLayout> catalog = [] {
        const std::string path = bundledLevelsPath();
        if (path.empty()) {
            SDL_Log("Tap Match: could not resolve base path for %s", kLevelsAsset);
            return std::vector<LevelLayout>{};
        }
        std::size_t size = 0;
        void* raw = SDL_LoadFile(path.c_str(), &size);
        if (raw == nullptr) {
            SDL_Log("Tap Match: failed to load %s: %s", path.c_str(), SDL_GetError());
            return std::vector<LevelLayout>{};
        }
        const SdlCharPtr data(static_cast<char*>(raw)); // owns the buffer (freed via SDL_free)
        // SDL hands back a void*; casting it straight to uint8_t* is well-defined
        // (uint8_t is unsigned char) and needs no reinterpret_cast.
        const auto* first = static_cast<const std::uint8_t*>(raw);
        return parseTapMatchLevels(std::span<const std::uint8_t>(first, size));
    }();
    return catalog;
}

} // namespace og
