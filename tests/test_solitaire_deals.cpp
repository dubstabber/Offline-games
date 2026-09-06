#include "games/solitaire/KlondikeGame.hpp"
#include "games/solitaire/KlondikeSolver.hpp"
#include "games/solitaire/SolitaireDeals.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

using og::KlondikeGame;
using og::klondikeRules;
using og::KlondikeSolver;
using og::kSolitaireTierCount;
using og::parseSolitaireDeals;
using og::serializeSolitaireDeals;
using og::SolitaireDealLists;

// Lists survive a serialize -> parse round-trip, including empty tiers.
void testRoundTrip() {
    SolitaireDealLists lists;
    lists[0] = {5, 7, 100000, 0xFFFFFFFFU};
    lists[2] = {42};
    const std::vector<std::uint8_t> bytes = serializeSolitaireDeals(lists);
    assert(bytes.size() == 4 + (3 * 4) + (5 * 4));
    const SolitaireDealLists back = parseSolitaireDeals(bytes);
    assert(back == lists);
}

// Short or truncated input never reads out of range: it yields what is whole.
void testTolerance() {
    assert(parseSolitaireDeals({}) == SolitaireDealLists{});
    SolitaireDealLists lists;
    lists[0] = {1, 2, 3};
    lists[1] = {9, 8};
    std::vector<std::uint8_t> bytes = serializeSolitaireDeals(lists);
    // header 4 + tier 0 (4 + 12) + tier 1 count 4 + one seed 4 = 28: cut into
    // tier 1's second seed.
    bytes.resize(30);
    const SolitaireDealLists back = parseSolitaireDeals(bytes);
    assert(back[0] == lists[0]);
    assert(back[1].empty()); // the truncated tier is dropped whole
    assert(back[2].empty());
    const std::vector<std::uint8_t> justHeader{3, 0, 0, 0};
    assert(parseSolitaireDeals(justHeader) == SolitaireDealLists{});
}

// The bundled asset holds a healthy list per tier, and its cheapest seed per
// tier really is winnable under that tier's rules.
void testBundledDeals() {
    std::ifstream in(SOLITAIRE_DEALS_PATH, std::ios::binary);
    assert(in && "assets/solitaire/deals.bytes missing");
    const std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(in), {});
    const SolitaireDealLists lists = parseSolitaireDeals(bytes);
    for (int tier = 0; tier < kSolitaireTierCount; ++tier) {
        const std::vector<std::uint32_t>& list = lists[static_cast<std::size_t>(tier)];
        std::printf("tier %d: %zu deals\n", tier, list.size());
        assert(list.size() >= 100);
        const KlondikeGame game(klondikeRules(tier), list.front());
        const KlondikeSolver::Result result = KlondikeSolver::solve(game, 50000);
        assert(result.solved);
    }
}

} // namespace

int main() {
    testRoundTrip();
    testTolerance();
    testBundledDeals();
    std::puts("solitaire_deals: all tests passed");
    return 0;
}
