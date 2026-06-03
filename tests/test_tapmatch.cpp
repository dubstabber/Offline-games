#include "games/tapmatch/TapMatchBoard.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

namespace {

using og::TapMatchBoard;
using GenParams = TapMatchBoard::GenParams;
using Result = TapMatchBoard::Result;

const std::array<GenParams, 3> kDifficulties = {
    GenParams{.iconVariety = 4,
              .layers = 2,
              .tileCount = 18,
              .holderBudget = 5,
              .gridWidth = 9,
              .gridHeight = 11},
    GenParams{.iconVariety = 6,
              .layers = 3,
              .tileCount = 36,
              .holderBudget = 6,
              .gridWidth = 9,
              .gridHeight = 11},
    GenParams{.iconVariety = 8,
              .layers = 4,
              .tileCount = 54,
              .holderBudget = 7,
              .gridWidth = 9,
              .gridHeight = 11},
};

// Each board's tiles, and each icon's copies, must come in groups of three so
// the holder can always clear out.
void testMultiplesOfThree() {
    for (const auto& params : kDifficulties) {
        for (std::uint64_t seed = 0; seed < 60; ++seed) {
            const TapMatchBoard board(params, seed);
            assert(board.tiles().size() % 3 == 0);
            std::array<int, TapMatchBoard::kMaxIcons> counts{};
            for (const auto& tile : board.tiles()) {
                assert(tile.icon >= 0 && tile.icon < TapMatchBoard::kMaxIcons);
                ++counts[static_cast<std::size_t>(tile.icon)];
            }
            for (const int count : counts) {
                assert(count % 3 == 0);
            }
        }
    }
}

// The headline guarantee: the generated solution order clears the whole board,
// every tap is on an accessible tile, the holder never exceeds 7, and the game
// is not "over" until the very last tile.
void testSolvable() {
    for (const auto& params : kDifficulties) {
        for (std::uint64_t seed = 0; seed < 300; ++seed) {
            TapMatchBoard board(params, seed);
            const std::vector<int> order = board.solutionOrder();
            const std::size_t n = board.tiles().size();
            assert(order.size() == n);

            // The order is a permutation of all tile ids.
            std::vector<char> seen(n, 0);
            for (const int id : order) {
                assert(id >= 0 && static_cast<std::size_t>(id) < n);
                assert(seen[static_cast<std::size_t>(id)] == 0);
                seen[static_cast<std::size_t>(id)] = 1;
            }

            for (std::size_t k = 0; k < n; ++k) {
                assert(!board.isOver()); // Won only on the last tap
                assert(board.isAccessible(order[k]));
                assert(board.holderSize() <= TapMatchBoard::kHolderCapacity);
                const bool ok = board.tapTile(order[k]);
                assert(ok);
                assert(board.holderSize() <= TapMatchBoard::kHolderCapacity);
            }
            assert(board.result() == Result::Won);
            assert(board.remaining() == 0);
            assert(board.holderSize() == 0);
        }
    }
}

// Covered tiles cannot be tapped, and every starting board has at least one free
// tile and (for layered boards) at least one covered tile.
void testCoveredTilesRejected() {
    const GenParams hard = kDifficulties[2];
    for (std::uint64_t seed = 0; seed < 40; ++seed) {
        TapMatchBoard board(hard, seed);
        bool anyAccessible = false;
        int coveredId = -1;
        for (const auto& tile : board.tiles()) {
            if (board.isAccessible(tile.id)) {
                anyAccessible = true;
            } else if (!tile.removed) {
                coveredId = tile.id;
            }
        }
        assert(anyAccessible);
        assert(coveredId >= 0); // a 4-layer pyramid always covers something

        const int remaining = board.remaining();
        const int holderSize = board.holderSize();
        assert(!board.tapTile(coveredId));
        assert(board.remaining() == remaining);
        assert(board.holderSize() == holderSize);
    }
}

// A minimal board: three of one fruit on a single layer. Tapping all three fills
// the holder to three, then clears it and wins.
void testTripleClearsAndWins() {
    TapMatchBoard board(GenParams{.iconVariety = 1,
                                  .layers = 1,
                                  .tileCount = 3,
                                  .holderBudget = 3,
                                  .gridWidth = 9,
                                  .gridHeight = 11},
                        123);
    assert(board.tiles().size() == 3);
    for (const auto& tile : board.tiles()) {
        assert(tile.icon == 0);
        assert(board.isAccessible(tile.id));
    }
    assert(board.tapTile(0));
    assert(board.holderSize() == 1);
    assert(!board.isOver());
    assert(board.tapTile(1));
    assert(board.holderSize() == 2);
    assert(!board.isOver());
    assert(board.tapTile(2));
    assert(board.holderSize() == 0); // the triple cleared
    assert(board.result() == Result::Won);
    assert(board.remaining() == 0);
}

// Same params + seed must yield an identical board and solution.
void testDeterministic() {
    const GenParams params = kDifficulties[1];
    const TapMatchBoard a(params, 999);
    const TapMatchBoard b(params, 999);
    assert(a.tiles().size() == b.tiles().size());
    for (std::size_t i = 0; i < a.tiles().size(); ++i) {
        const auto& ta = a.tiles()[i];
        const auto& tb = b.tiles()[i];
        assert(ta.id == tb.id && ta.icon == tb.icon && ta.layer == tb.layer);
        assert(ta.x == tb.x && ta.y == tb.y);
    }
    assert(a.solutionOrder() == b.solutionOrder());
}

// Out-of-range ids, re-tapping a removed tile, and taps after game-over all do
// nothing.
void testInvalidTaps() {
    TapMatchBoard board(kDifficulties[1], 7);
    const int n = static_cast<int>(board.tiles().size());
    assert(!board.tapTile(-1));
    assert(!board.tapTile(n));
    assert(!board.tapTile(n + 1000));

    int accessible = -1;
    for (const auto& tile : board.tiles()) {
        if (board.isAccessible(tile.id)) {
            accessible = tile.id;
            break;
        }
    }
    assert(accessible >= 0);
    assert(board.tapTile(accessible));
    assert(!board.tapTile(accessible)); // already removed
}

// Random legal play: a move always exists while playing, the holder never spills
// past 7, the game always terminates, and the result is consistent (a win empties
// everything; a loss happens exactly when the holder is full). Random play loses
// most boards, so this also exercises the lose path and post-game-over rejection.
void testRandomPlayInvariants() {
    int losses = 0;
    int wins = 0;
    for (std::uint64_t seed = 0; seed < 300; ++seed) {
        const GenParams& params = kDifficulties[seed % kDifficulties.size()];
        TapMatchBoard board(params, seed);
        std::mt19937 rng(static_cast<std::uint32_t>(seed) ^ 0x9E3779B9U);

        int guard = 0;
        while (!board.isOver()) {
            std::vector<int> accessible;
            for (const auto& tile : board.tiles()) {
                if (board.isAccessible(tile.id)) {
                    accessible.push_back(tile.id);
                }
            }
            assert(!accessible.empty()); // a move always exists mid-game
            std::uniform_int_distribution<std::size_t> pick(0, accessible.size() - 1);
            const bool ok = board.tapTile(accessible[pick(rng)]);
            assert(ok);
            assert(board.holderSize() <= TapMatchBoard::kHolderCapacity);
            ++guard;
            assert(guard < 100000); // must terminate
        }

        if (board.result() == Result::Won) {
            ++wins;
            assert(board.remaining() == 0);
            assert(board.holderSize() == 0);
        } else {
            ++losses;
            assert(board.holderSize() == TapMatchBoard::kHolderCapacity);
        }

        // No taps register once the game is over.
        for (const auto& tile : board.tiles()) {
            if (!tile.removed) {
                assert(!board.tapTile(tile.id));
                break;
            }
        }
    }
    assert(losses > 0); // random play should fail some boards
    (void)wins;
}

} // namespace

int main() {
    testMultiplesOfThree();
    testSolvable();
    testCoveredTilesRejected();
    testTripleClearsAndWins();
    testDeterministic();
    testInvalidTaps();
    testRandomPlayInvariants();
    std::puts("All TapMatchBoard tests passed.");
    return 0;
}
