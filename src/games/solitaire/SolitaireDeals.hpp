#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace og {

// Solitaire ships a list of deal seeds per difficulty tier, every one verified
// winnable by KlondikeSolver under that tier's rules (see
// tools/gen_solitaire_deals.cpp), the way the original ships curated deals.
inline constexpr int kSolitaireTierCount = 3;
using SolitaireDealLists = std::array<std::vector<std::uint32_t>, kSolitaireTierCount>;

// Decode the deal-list blob. Format (little-endian):
//   [u32 tierCount] then per tier [u32 count][count x u32 seed]
// Pure and tolerant: a short or malformed buffer yields empty lists, never a
// read out of range. Seeds within a tier are in the order the generator wrote
// them (cheapest-to-solve first).
[[nodiscard]] SolitaireDealLists parseSolitaireDeals(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::vector<std::uint8_t> serializeSolitaireDeals(const SolitaireDealLists& lists);

// The bundled lists, loaded once from assets/solitaire/deals.bytes next to the
// executable (via SDL_GetBasePath); empty lists if the asset is missing.
[[nodiscard]] const SolitaireDealLists& solitaireDeals();

// A seed to play at `tier`: one of the bundled winnable deals chosen by
// `pick` (any value; reduced modulo the list), or `pick` itself as a plain
// random deal when the tier has no list.
[[nodiscard]] std::uint32_t solitaireDealSeed(int tier, std::uint32_t pick);

} // namespace og
