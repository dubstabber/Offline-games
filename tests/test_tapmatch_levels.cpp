#include "games/tapmatch/TapMatchBoard.hpp"
#include "games/tapmatch/TapMatchLevels.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <span>
#include <vector>

namespace {

using og::LevelLayout;
using og::parseTapMatchLevels;
using og::TapMatchBoard;

// Decode two hand-built level records using the exact on-disk format and check
// every decoded field — the format spec lives in TapMatchLevels.hpp.
void testParserSynthetic() {
    // Headers: iconsCount<<3 | ((initStackSize-5)&3)<<1 | hasFTUE.
    //   0x1A -> iconsCount 3, initStackSize 6, no FTUE.
    //   0x10 -> iconsCount 2, initStackSize 5, no FTUE.
    // Tile u16: layer<<10 | gx<<6 | gy<<2 | xDelta<<1 | yDelta; fine = (2*gx+dx, 2*gy+dy).
    //   0x0044 -> layer0 gx1 gy1            -> (0, 2, 2)
    //   0x0084 -> layer0 gx2 gy1            -> (0, 4, 2)
    //   0x0447 -> layer1 gx1 gy1 dx1 dy1    -> (1, 3, 3)
    const std::vector<std::uint8_t> blob = {
        0x02, 0x00, 0x00, 0x00,                         // count = 2
        0x00, 0x00, 0x00, 0x00,                         // offset[1] = 0  (record at dataBase+0)
        0x08, 0x00, 0x00, 0x00,                         // offset[2] = 8  (record at dataBase+8)
        0x1A, 0x03, 0x44, 0x00, 0x84, 0x00, 0x47, 0x04, // level 1: header, n=3, A, B, C
        0x10, 0x03, 0x00, 0x00, 0x40, 0x00, 0x80, 0x00, // level 2: header, n=3, D, E, F
    };
    const std::vector<LevelLayout> levels = parseTapMatchLevels(blob);
    assert(levels.size() == 2);

    assert(levels[0].iconsCount == 3);
    assert(levels[0].initStackSize == 6);
    assert(levels[0].tiles.size() == 3);
    assert(levels[0].tiles[0].layer == 0 && levels[0].tiles[0].x == 2 && levels[0].tiles[0].y == 2);
    assert(levels[0].tiles[1].layer == 0 && levels[0].tiles[1].x == 4 && levels[0].tiles[1].y == 2);
    assert(levels[0].tiles[2].layer == 1 && levels[0].tiles[2].x == 3 && levels[0].tiles[2].y == 3);

    assert(levels[1].iconsCount == 2);
    assert(levels[1].initStackSize == 5);
    assert(levels[1].tiles.size() == 3);
    assert(levels[1].tiles[0].x == 0 && levels[1].tiles[0].y == 0);
    assert(levels[1].tiles[1].x == 2 && levels[1].tiles[1].y == 0);
    assert(levels[1].tiles[2].x == 4 && levels[1].tiles[2].y == 0);
}

// A short, empty, or zero-count buffer must not crash and yields nothing usable.
void testParserRobustness() {
    const std::vector<std::uint8_t> empty;
    assert(parseTapMatchLevels(empty).empty());

    const std::vector<std::uint8_t> zeroCount = {0x00, 0x00, 0x00, 0x00};
    assert(parseTapMatchLevels(zeroCount).empty());

    // Claims 5 levels but has no offset table: the first offset read fails, stop.
    const std::vector<std::uint8_t> truncated = {0x05, 0x00, 0x00, 0x00, 0x01};
    assert(parseTapMatchLevels(truncated).empty());
}

[[nodiscard]] std::vector<std::uint8_t> readFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return {};
    }
    f.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(size);
    f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    return bytes;
}

// The headline correctness gate: decode the real bundled level set and assert
// every level is well-formed (multiple of 3, in-bounds) AND that the engine
// produces a guaranteed-win order for each authored board.
void testRealAssetSolvable() {
#ifdef TAPMATCH_LEVELS_PATH
    const std::vector<std::uint8_t> bytes = readFile(TAPMATCH_LEVELS_PATH);
    if (bytes.empty()) {
        std::printf("WARNING: could not read %s; skipping real-asset gate\n", TAPMATCH_LEVELS_PATH);
        return;
    }
    const std::vector<LevelLayout> levels = parseTapMatchLevels(bytes);
    assert(!levels.empty());
    std::printf("Decoded %zu Tap Match levels from the bundled asset.\n", levels.size());

    for (std::size_t i = 0; i < levels.size(); ++i) {
        const LevelLayout& level = levels[i];
        assert(!level.tiles.empty());        // no empty boards
        assert(level.tiles.size() % 3 == 0); // triple-tile invariant
        for (const auto& tile : level.tiles) {
            // 4-bit grid coords + half-cell delta -> fine coords in [0, 31].
            assert(tile.layer >= 0 && tile.layer < 64);
            assert(tile.x >= 0 && tile.x <= 31);
            assert(tile.y >= 0 && tile.y <= 31);
        }

        // The board the player gets must be winnable: replay the generated
        // solution order and confirm it clears the board within the holder.
        const TapMatchBoard board(level, static_cast<std::uint64_t>(i) + 1);
        const std::vector<int>& order = board.solutionOrder();
        assert(order.size() == board.tiles().size());

        TapMatchBoard play(level, static_cast<std::uint64_t>(i) + 1);
        for (const int id : play.solutionOrder()) {
            assert(!play.isOver());
            assert(play.isAccessible(id));
            assert(play.holderSize() <= TapMatchBoard::kHolderCapacity);
            assert(play.tapTile(id));
            assert(play.holderSize() <= TapMatchBoard::kHolderCapacity);
        }
        assert(play.result() == TapMatchBoard::Result::Won);
        assert(play.remaining() == 0);
        assert(play.holderSize() == 0);
    }
    std::printf("All %zu levels are multiple-of-3, in-bounds, and solvable.\n", levels.size());
#else
    std::printf("TAPMATCH_LEVELS_PATH not defined; skipping real-asset gate.\n");
#endif
}

// The difficulty tiers must form a partition of the catalog, each ordered
// easiest-first, with non-decreasing complexity across tiers (so Easy < Hard).
void testTiers() {
#ifdef TAPMATCH_LEVELS_PATH
    const std::vector<std::uint8_t> bytes = readFile(TAPMATCH_LEVELS_PATH);
    const std::vector<LevelLayout> levels = parseTapMatchLevels(bytes);
    if (levels.empty()) {
        return;
    }
    const std::array<std::vector<int>, og::kTapMatchTierCount> tiers =
        og::tapMatchTierPartition(levels);

    std::size_t sum = 0;
    std::size_t prevTierMax = 0;
    for (int t = 0; t < og::kTapMatchTierCount; ++t) {
        const std::vector<int>& tier = tiers[static_cast<std::size_t>(t)];
        assert(!tier.empty());
        sum += tier.size();
        for (std::size_t i = 1; i < tier.size(); ++i) {
            assert(levels[static_cast<std::size_t>(tier[i - 1])].tiles.size() <=
                   levels[static_cast<std::size_t>(tier[i])].tiles.size());
        }
        const std::size_t lo = levels[static_cast<std::size_t>(tier.front())].tiles.size();
        const std::size_t hi = levels[static_cast<std::size_t>(tier.back())].tiles.size();
        if (t > 0) {
            assert(lo >= prevTierMax); // this tier starts no easier than the last ended
        }
        prevTierMax = hi;
    }
    assert(sum == levels.size()); // every level lands in exactly one tier
    std::printf("Difficulty tier sizes: %zu / %zu / %zu (sum %zu).\n", tiers[0].size(),
                tiers[1].size(), tiers[2].size(), sum);
#endif
}

} // namespace

int main() {
    testParserSynthetic();
    testParserRobustness();
    testRealAssetSolvable();
    testTiers();
    std::puts("All TapMatchLevels tests passed.");
    return 0;
}
