#include "games/solitaire/SolitaireDeals.hpp"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <SDL3/SDL.h>
#include <string>

namespace og {
namespace {

// Bounds are checked up front, so the span indexing below is safe.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
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

void writeU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>((v >> shift) & 0xFFU));
    }
}

constexpr const char* kDealsAsset = "assets/solitaire/deals.bytes";

[[nodiscard]] std::vector<std::uint8_t> readBundledDeals() {
    const char* base = SDL_GetBasePath();
    if (base == nullptr) {
        return {};
    }
    const std::string path = std::string(base) + kDealsAsset;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        SDL_Log("Solitaire deals asset not found at %s; using random deals", path.c_str());
        return {};
    }
    std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(in), {});
    return bytes;
}

} // namespace

SolitaireDealLists parseSolitaireDeals(std::span<const std::uint8_t> bytes) {
    SolitaireDealLists lists;
    std::uint32_t tierCount = 0;
    std::size_t pos = 0;
    if (!readU32(bytes, pos, tierCount)) {
        return lists;
    }
    pos += 4;
    for (std::uint32_t tier = 0; tier < tierCount && tier < kSolitaireTierCount; ++tier) {
        std::uint32_t count = 0;
        if (!readU32(bytes, pos, count)) {
            break;
        }
        pos += 4;
        std::vector<std::uint32_t>& list = lists.at(tier);
        for (std::uint32_t i = 0; i < count; ++i) {
            std::uint32_t seed = 0;
            if (!readU32(bytes, pos, seed)) {
                list.clear(); // a truncated tier is dropped whole
                return lists;
            }
            pos += 4;
            list.push_back(seed);
        }
    }
    return lists;
}

std::vector<std::uint8_t> serializeSolitaireDeals(const SolitaireDealLists& lists) {
    std::vector<std::uint8_t> out;
    writeU32(out, kSolitaireTierCount);
    for (const std::vector<std::uint32_t>& list : lists) {
        writeU32(out, static_cast<std::uint32_t>(list.size()));
        for (const std::uint32_t seed : list) {
            writeU32(out, seed);
        }
    }
    return out;
}

const SolitaireDealLists& solitaireDeals() {
    static const SolitaireDealLists lists = parseSolitaireDeals(readBundledDeals());
    return lists;
}

std::uint32_t solitaireDealSeed(int tier, std::uint32_t pick) {
    if (tier < 0 || tier >= kSolitaireTierCount) {
        return pick;
    }
    const std::vector<std::uint32_t>& list = solitaireDeals().at(static_cast<std::size_t>(tier));
    if (list.empty()) {
        return pick;
    }
    return list.at(static_cast<std::size_t>(pick % list.size()));
}

} // namespace og
