#include "games/blockfill/BlockFillLevels.hpp"

#include "core/Sdl.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <SDL3/SDL.h>
#include <span>
#include <string>
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

constexpr std::uint32_t kMagic = 0x464B4C42; // "BLKF" little-endian
constexpr const char* kLevelsAsset = "assets/blockfill/levels.bytes";

// Decode one record at `pos`, advancing it past the record. Returns false (and
// leaves `pos` unspecified) on a truncated/out-of-range record.
[[nodiscard]] bool readRecord(std::span<const std::uint8_t> b, std::size_t& pos,
                              BlockFillLevel& out) {
    std::uint8_t w = 0;
    std::uint8_t h = 0;
    std::uint8_t sx = 0;
    std::uint8_t sy = 0;
    if (!readU8(b, pos, w) || !readU8(b, pos + 1, h) || !readU8(b, pos + 2, sx) ||
        !readU8(b, pos + 3, sy)) {
        return false;
    }
    pos += 4;
    const int cells = static_cast<int>(w) * static_cast<int>(h);
    if (w == 0 || h == 0) {
        return false;
    }
    const std::size_t maskBytes = (static_cast<std::size_t>(cells) + 7) / 8;

    out.width = w;
    out.height = h;
    out.startX = sx;
    out.startY = sy;
    out.playable.assign(static_cast<std::size_t>(cells), 0);
    for (int idx = 0; idx < cells; ++idx) {
        std::uint8_t maskByte = 0;
        if (!readU8(b, pos + (static_cast<std::size_t>(idx) / 8), maskByte)) {
            return false;
        }
        const bool missing = ((maskByte >> (idx % 8)) & 1U) != 0;
        out.playable.at(static_cast<std::size_t>(idx)) =
            missing ? std::uint8_t{0} : std::uint8_t{1}; // bit set = hole
    }
    pos += maskBytes;
    return true;
}

// Path to the bundled level blob next to the executable (empty if SDL can't tell).
[[nodiscard]] std::string bundledLevelsPath() {
    const char* base = SDL_GetBasePath();
    if (base == nullptr) {
        return {};
    }
    return std::string(base) + kLevelsAsset;
}

} // namespace

int BlockFillLevel::playableCount() const {
    return static_cast<int>(std::ranges::count(playable, std::uint8_t{1}));
}

std::array<std::vector<BlockFillLevel>, kBlockFillTierCount>
parseBlockFillLevels(std::span<const std::uint8_t> bytes) {
    std::array<std::vector<BlockFillLevel>, kBlockFillTierCount> tiers;

    std::uint32_t magic = 0;
    std::uint16_t version = 0;
    std::uint16_t tierCount = 0;
    if (!readU32(bytes, 0, magic) || magic != kMagic || !readU16(bytes, 4, version) ||
        version == 0 || !readU16(bytes, 6, tierCount) || tierCount != kBlockFillTierCount) {
        return tiers;
    }

    std::array<std::uint16_t, kBlockFillTierCount> counts{};
    for (int t = 0; t < kBlockFillTierCount; ++t) {
        if (!readU16(bytes, 8 + (static_cast<std::size_t>(t) * 2),
                     counts.at(static_cast<std::size_t>(t)))) {
            return tiers;
        }
    }

    std::size_t pos = 16; // records begin right after the fixed-size header
    for (int t = 0; t < kBlockFillTierCount; ++t) {
        const std::uint16_t want = counts.at(static_cast<std::size_t>(t));
        tiers.at(static_cast<std::size_t>(t)).reserve(want);
        for (std::uint16_t k = 0; k < want; ++k) {
            BlockFillLevel level;
            if (!readRecord(bytes, pos, level)) {
                return tiers; // truncated record: keep the fully-decoded levels
            }
            tiers.at(static_cast<std::size_t>(t)).push_back(std::move(level));
        }
    }
    return tiers;
}

const std::array<std::vector<BlockFillLevel>, kBlockFillTierCount>& blockFillLevels() {
    static const std::array<std::vector<BlockFillLevel>, kBlockFillTierCount> catalog = [] {
        const std::string path = bundledLevelsPath();
        if (path.empty()) {
            SDL_Log("Block Fill: could not resolve base path for %s", kLevelsAsset);
            return std::array<std::vector<BlockFillLevel>, kBlockFillTierCount>{};
        }
        std::size_t size = 0;
        void* raw = SDL_LoadFile(path.c_str(), &size);
        if (raw == nullptr) {
            SDL_Log("Block Fill: failed to load %s: %s", path.c_str(), SDL_GetError());
            return std::array<std::vector<BlockFillLevel>, kBlockFillTierCount>{};
        }
        const SdlCharPtr data(static_cast<char*>(raw)); // owns the buffer (freed via SDL_free)
        const auto* first = static_cast<const std::uint8_t*>(raw);
        return parseBlockFillLevels(std::span<const std::uint8_t>(first, size));
    }();
    return catalog;
}

int blockFillTierSize(int tier) {
    if (tier < 0 || tier >= kBlockFillTierCount) {
        return 0;
    }
    return static_cast<int>(blockFillLevels().at(static_cast<std::size_t>(tier)).size());
}

const BlockFillLevel* blockFillTierLevel(int tier, int level) {
    if (tier < 0 || tier >= kBlockFillTierCount) {
        return nullptr;
    }
    const std::vector<BlockFillLevel>& pool = blockFillLevels().at(static_cast<std::size_t>(tier));
    if (pool.empty()) {
        return nullptr;
    }
    const int clamped = std::clamp(level, 1, static_cast<int>(pool.size()));
    return &pool.at(static_cast<std::size_t>(clamped - 1));
}

} // namespace og
