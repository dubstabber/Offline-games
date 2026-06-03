#include "games/blockfill/BlockFillBoard.hpp"
#include "games/blockfill/BlockFillLevels.hpp"

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

using og::BlockFillBoard;
using og::BlockFillLevel;
using og::kBlockFillTierCount;
using og::parseBlockFillLevels;

// A 3x3 all-playable board with the rope starting top-left.
[[nodiscard]] BlockFillBoard makeFullBoard() {
    const std::vector<std::uint8_t> playable(9, 1);
    return {3, 3, playable, 0, 0};
}

// Walk a full snake path across the 3x3, asserting each extend, then solved.
void testExtendAndSolve() {
    BlockFillBoard board = makeFullBoard();
    assert(board.playableCount() == 9);
    assert(board.pathLength() == 1);
    assert((board.head() == BlockFillBoard::Cell{0, 0}));
    assert(!board.isSolved());

    // Boustrophedon: (0,0)->(1,0)->(2,0)->(2,1)->(1,1)->(0,1)->(0,2)->(1,2)->(2,2).
    const std::array<BlockFillBoard::Cell, 8> route = {
        {{1, 0}, {2, 0}, {2, 1}, {1, 1}, {0, 1}, {0, 2}, {1, 2}, {2, 2}}};
    for (const auto& c : route) {
        assert(board.stepTo(c.x, c.y));
        assert(board.head() == c);
        assert(board.pathContains(c.x, c.y));
    }
    assert(board.pathLength() == 9);
    assert(board.isSolved());
}

// Diagonal / out-of-bounds / non-adjacent steps are rejected.
void testIllegalSteps() {
    BlockFillBoard board = makeFullBoard();
    assert(!board.stepTo(1, 1));  // diagonal from (0,0)
    assert(!board.stepTo(-1, 0)); // out of bounds
    assert(!board.stepTo(2, 2));  // not adjacent
    assert(board.pathLength() == 1);
}

// Stepping back onto the pre-head cell retracts; a fresh cell re-extends.
void testRetract() {
    BlockFillBoard board = makeFullBoard();
    assert(board.stepTo(1, 0));
    assert(board.stepTo(1, 1));
    assert(board.pathLength() == 3);
    assert(board.stepTo(1, 0)); // back onto the pre-head cell -> retract
    assert(board.pathLength() == 2);
    assert(!board.pathContains(1, 1));
    assert(board.stepTo(2, 0)); // re-extend elsewhere
    assert(board.pathLength() == 3);
}

// An adjacent cell already on the rope (but not the pre-head) cannot be revisited.
void testNoRevisit() {
    BlockFillBoard board = makeFullBoard();
    assert(board.stepTo(0, 1));
    assert(board.stepTo(1, 1));
    assert(board.stepTo(1, 0)); // head is now (1,0)
    // (0,0) is adjacent to (1,0) and on the rope (start), but is not the pre-head
    // cell (that's (1,1)), so it must be rejected, not retracted.
    assert(!board.stepTo(0, 0));
    assert(board.pathLength() == 4);
}

// A fast drag across a straight run fills every cell in between.
void testDragToward() {
    BlockFillBoard board = makeFullBoard();
    const int changed = board.dragToward(2, 0); // (0,0) -> (1,0) -> (2,0)
    assert(changed == 2);
    assert(board.pathLength() == 3);
    assert((board.head() == BlockFillBoard::Cell{2, 0}));
}

// Touching any rope cell truncates the rope to it (cut anywhere, not just by one).
void testTruncate() {
    BlockFillBoard board = makeFullBoard();
    const std::array<BlockFillBoard::Cell, 4> route = {{{1, 0}, {2, 0}, {2, 1}, {2, 2}}};
    for (const auto& c : route) {
        assert(board.stepTo(c.x, c.y));
    }
    assert(board.pathLength() == 5); // (0,0)->(1,0)->(2,0)->(2,1)->(2,2)

    // Cut back to a mid cell far from the head: everything after it is dropped.
    assert(board.truncateTo(1, 0));
    assert(board.pathLength() == 2);
    assert((board.head() == BlockFillBoard::Cell{1, 0}));
    assert(!board.pathContains(2, 0));
    assert(!board.pathContains(2, 2));

    // Truncating to the current head changes nothing but still reports success.
    assert(board.truncateTo(1, 0));
    assert(board.pathLength() == 2);

    // A cell not on the rope is rejected and leaves the rope untouched.
    assert(!board.truncateTo(0, 2));
    assert(board.pathLength() == 2);

    // Tapping the start clears the rope back to just the start; re-extend works.
    assert(board.truncateTo(0, 0));
    assert(board.pathLength() == 1);
    assert((board.head() == BlockFillBoard::Cell{0, 0}));
    assert(board.stepTo(0, 1));
    assert(board.pathLength() == 2);
}

// Holes are not playable; the rope routes around them.
void testHole() {
    std::vector<std::uint8_t> playable(9, 1);
    playable[(1 * 3) + 1] = 0; // centre (1,1) is a hole
    BlockFillBoard board(3, 3, playable, 0, 0);
    assert(board.playableCount() == 8);
    assert(!board.isPlayable(1, 1));
    assert((board.stepTo(1, 0) && board.head() == BlockFillBoard::Cell{1, 0}));
    // From (1,0) you cannot step down into the hole.
    assert(!board.stepTo(1, 1));
    // Ring path around the hole: start (0,0) + (1,0) + these six covers all 8.
    const std::array<BlockFillBoard::Cell, 6> ring = {
        {{2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}}};
    for (const auto& c : ring) {
        assert(board.stepTo(c.x, c.y));
    }
    assert(board.isSolved());
}

// Decode a hand-built blob using the exact on-disk format (see BlockFillLevels.hpp).
void testParserSynthetic() {
    const std::vector<std::uint8_t> blob = {
        0x42, 0x4C, 0x4B, 0x46, // magic "BLKF"
        0x01, 0x00,             // version 1
        0x04, 0x00,             // tierCount 4
        0x01, 0x00,             // count[0] = 1
        0x00, 0x00,             // count[1] = 0
        0x00, 0x00,             // count[2] = 0
        0x00, 0x00,             // count[3] = 0
        0x02, 0x02, 0x00, 0x00, // w=2 h=2 startX=0 startY=0
        0x08,                   // mask: bit 3 set -> cell (1,1) is a hole
    };
    const std::array<std::vector<BlockFillLevel>, kBlockFillTierCount> tiers =
        parseBlockFillLevels(blob);
    assert(tiers[0].size() == 1);
    assert(tiers[1].empty() && tiers[2].empty() && tiers[3].empty());

    const BlockFillLevel& lv = tiers[0][0];
    assert(lv.width == 2 && lv.height == 2);
    assert(lv.startX == 0 && lv.startY == 0);
    assert(lv.playable.size() == 4);
    assert(lv.playable[0] == 1 && lv.playable[1] == 1 && lv.playable[2] == 1);
    assert(lv.playable[3] == 0); // the hole
    assert(lv.playableCount() == 3);
}

// Empty / bad-magic / wrong-tier-count / truncated buffers must not crash.
void testParserRobustness() {
    auto allEmpty = [](const std::array<std::vector<BlockFillLevel>, kBlockFillTierCount>& t) {
        for (const auto& tier : t) {
            if (!tier.empty()) {
                return false;
            }
        }
        return true;
    };
    assert(allEmpty(parseBlockFillLevels(std::vector<std::uint8_t>{})));
    assert(allEmpty(parseBlockFillLevels(std::vector<std::uint8_t>{0, 0, 0, 0, 1, 0, 4, 0})));

    // Valid header that promises one record but supplies none: tier stays empty.
    const std::vector<std::uint8_t> truncated = {0x42, 0x4C, 0x4B, 0x46, 0x01, 0x00, 0x04, 0x00,
                                                 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    assert(allEmpty(parseBlockFillLevels(truncated)));
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

// Every playable cell must be reachable from the start through orthogonal moves
// over playable cells — a Hamiltonian path can't exist on a disconnected board.
[[nodiscard]] bool playableConnected(const BlockFillLevel& lv) {
    const int w = lv.width;
    const int h = lv.height;
    std::vector<std::uint8_t> seen(static_cast<std::size_t>(w * h), 0);
    std::vector<std::pair<int, int>> stack = {{lv.startX, lv.startY}};
    seen[static_cast<std::size_t>((lv.startY * w) + lv.startX)] = 1;
    int reached = 0;
    while (!stack.empty()) {
        const auto [x, y] = stack.back();
        stack.pop_back();
        ++reached;
        const std::array<std::pair<int, int>, 4> nbrs = {
            {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}}};
        for (const auto& [nx, ny] : nbrs) {
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) {
                continue;
            }
            const std::size_t idx = static_cast<std::size_t>((ny * w) + nx);
            if (lv.playable[idx] != 0 && seen[idx] == 0) {
                seen[idx] = 1;
                stack.emplace_back(nx, ny);
            }
        }
    }
    return reached == lv.playableCount();
}

// Decode the real bundled asset and assert every board is well-formed.
void testRealAsset() {
#ifdef BLOCKFILL_LEVELS_PATH
    const std::vector<std::uint8_t> bytes = readFile(BLOCKFILL_LEVELS_PATH);
    if (bytes.empty()) {
        std::printf("WARNING: could not read %s; skipping real-asset gate\n",
                    BLOCKFILL_LEVELS_PATH);
        return;
    }
    const std::array<std::vector<BlockFillLevel>, kBlockFillTierCount> tiers =
        parseBlockFillLevels(bytes);
    std::size_t total = 0;
    for (int t = 0; t < kBlockFillTierCount; ++t) {
        const std::vector<BlockFillLevel>& pool = tiers[static_cast<std::size_t>(t)];
        assert(!pool.empty());
        total += pool.size();
        for (const BlockFillLevel& lv : pool) {
            assert(lv.width >= 1 && lv.width <= 15);
            assert(lv.height >= 1 && lv.height <= 15);
            assert(lv.playable.size() == static_cast<std::size_t>(lv.width * lv.height));
            assert(lv.startX >= 0 && lv.startX < lv.width);
            assert(lv.startY >= 0 && lv.startY < lv.height);
            assert(lv.playable[static_cast<std::size_t>((lv.startY * lv.width) + lv.startX)] != 0);
            assert(lv.playableCount() >= 2);
            assert(playableConnected(lv));
        }
    }
    std::printf("Decoded %zu Block Fill levels (%zu/%zu/%zu/%zu); all well-formed and connected.\n",
                total, tiers[0].size(), tiers[1].size(), tiers[2].size(), tiers[3].size());
#else
    std::printf("BLOCKFILL_LEVELS_PATH not defined; skipping real-asset gate.\n");
#endif
}

} // namespace

int main() {
    testExtendAndSolve();
    testIllegalSteps();
    testRetract();
    testNoRevisit();
    testDragToward();
    testTruncate();
    testHole();
    testParserSynthetic();
    testParserRobustness();
    testRealAsset();
    std::puts("All Block Fill tests passed.");
    return 0;
}
