#include "games/Difficulty.hpp"
#include "games/mahjong/MahjongBoard.hpp"
#include "games/mahjong/MahjongLayouts.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace {

using og::Difficulty;
using og::MahjongBoard;
using og::MahjongLayout;
using Slot = MahjongBoard::Slot;

constexpr std::array<Difficulty, 3> kTiers{Difficulty::Easy, Difficulty::Medium, Difficulty::Hard};

// Three tiles in a row with a fourth on top of the middle one.
[[nodiscard]] MahjongBoard makeRowBoard() {
    const std::vector<Slot> slots{{0, 0, 0}, {2, 0, 0}, {4, 0, 0}, {2, 0, 1}};
    return {slots, 7};
}

void testFreeRule() {
    const MahjongBoard board = makeRowBoard();
    assert(board.tileCount() == 4);
    assert(board.remaining() == 4);
    assert(board.width() == 6 && board.height() == 2 && board.layers() == 2);
    assert(board.isFree(0));  // left end: nothing on its left
    assert(!board.isFree(1)); // covered, and walled in on both sides
    assert(board.isFree(2));  // right end
    assert(board.isFree(3));  // on top
    assert(!board.isFree(4)); // no such tile
}

// Replay the dealer's own clearing order through tap(): every pair must be
// free and matching when its turn comes, and the board ends cleared.
void replaySolution(MahjongBoard& board) {
    const std::vector<std::pair<int, int>> solution = board.solution();
    assert(static_cast<int>(solution.size()) * 2 == board.remaining());
    for (const auto& [a, b] : solution) {
        assert(board.isFree(a));
        assert(board.tap(a) == MahjongBoard::Tap::Selected);
        assert(board.selected() == a);
        assert(board.isFree(b));
        assert(board.matches(a, b));
        assert(board.tap(b) == MahjongBoard::Tap::Matched);
        assert(board.selected() == -1);
    }
    assert(board.isWon());
    assert(board.remaining() == 0);
    assert(!board.hasMoves());
}

void testTapSemantics() {
    MahjongBoard board = makeRowBoard();
    // A dealt pair is always free to start with: take it from the hint.
    const auto [a, b] = board.hint();
    assert(a >= 0 && b >= 0 && board.matches(a, b));
    assert(board.tap(1) == MahjongBoard::Tap::Blocked); // the walled-in middle
    assert(board.selected() == -1);
    assert(board.tap(a) == MahjongBoard::Tap::Selected);
    assert(board.tap(a) == MahjongBoard::Tap::Deselected);
    assert(board.tap(a) == MahjongBoard::Tap::Selected);
    assert(board.tap(b) == MahjongBoard::Tap::Matched);
    assert(board.remaining() == 2);
    assert(board.tap(a) == MahjongBoard::Tap::Ignored); // gone
    assert(board.canUndo());
    assert(board.undo());
    assert(board.remaining() == 4);
    assert(!board.tiles()[static_cast<std::size_t>(a)].removed);
    assert(board.selected() == -1);
    assert(!board.undo()); // history exhausted
    replaySolution(board);
}

// Every group is dealt in pairs (never more than a full set of four), and
// flowers / seasons show four different faces.
void checkDeal(const MahjongBoard& board) {
    std::array<int, MahjongBoard::kGroupCount> counts{};
    std::array<int, MahjongBoard::kGroupCount> faceMask{};
    for (const MahjongBoard::Tile& t : board.tiles()) {
        assert(t.group >= 0 && t.group < MahjongBoard::kGroupCount);
        ++counts.at(static_cast<std::size_t>(t.group));
        if (t.group == MahjongBoard::kFlowers || t.group == MahjongBoard::kSeasons) {
            assert(t.face >= 0 && t.face < MahjongBoard::kCopies);
            faceMask.at(static_cast<std::size_t>(t.group)) |= 1 << t.face;
        } else {
            assert(t.face == 0);
        }
    }
    for (std::size_t g = 0; g < counts.size(); ++g) {
        assert(counts.at(g) % 2 == 0 && counts.at(g) <= MahjongBoard::kCopies);
        if (counts.at(g) == MahjongBoard::kCopies &&
            (g == MahjongBoard::kFlowers || g == MahjongBoard::kSeasons)) {
            assert(faceMask.at(g) == 0xF);
        }
    }
}

// Layouts: even, non-overlapping within a layer, every raised tile rests on
// something, and the whole thing fits the portrait board.
void checkLayout(const MahjongLayout& layout) {
    assert(layout.slots.size() % 2 == 0);
    assert(!layout.slots.empty());
    int maxX = 0;
    int maxY = 0;
    for (std::size_t i = 0; i < layout.slots.size(); ++i) {
        const Slot& a = layout.slots[i];
        assert(a.x >= 0 && a.y >= 0 && a.z >= 0);
        maxX = std::max(maxX, a.x + 2);
        maxY = std::max(maxY, a.y + 2);
        bool supported = a.z == 0;
        for (std::size_t j = 0; j < layout.slots.size(); ++j) {
            if (i == j) {
                continue;
            }
            const Slot& b = layout.slots[j];
            const bool overlap = std::abs(a.x - b.x) < 2 && std::abs(a.y - b.y) < 2;
            assert(!(overlap && a.z == b.z));
            supported = supported || (overlap && b.z == a.z - 1);
        }
        assert(supported);
    }
    assert(maxX <= 20 && maxY <= 24);
}

void testLayoutsAndDeals() {
    for (const Difficulty tier : kTiers) {
        assert(!og::mahjongLayouts(tier).empty());
        for (const MahjongLayout& layout : og::mahjongLayouts(tier)) {
            checkLayout(layout);
            for (std::uint32_t seed = 1; seed <= 6; ++seed) {
                MahjongBoard board(layout.slots, seed);
                assert(board.tileCount() == static_cast<int>(layout.slots.size()));
                assert(!board.solution().empty()); // the dealer succeeded
                checkDeal(board);
                assert(board.hasMoves());
                replaySolution(board);
            }
        }
    }
    // The full set uses every group exactly four times.
    const MahjongLayout& turtle = og::mahjongLayouts(Difficulty::Hard)[0];
    assert(turtle.slots.size() == 144);
    MahjongBoard full(turtle.slots, 3);
    std::array<int, MahjongBoard::kGroupCount> counts{};
    for (const MahjongBoard::Tile& t : full.tiles()) {
        ++counts.at(static_cast<std::size_t>(t.group));
    }
    for (const int c : counts) {
        assert(c == MahjongBoard::kCopies);
    }
}

// Shuffling keeps the remaining tiles' kinds (as a multiset) and positions,
// counts as a move that undo can revert, and leaves a clearable board.
void testShuffle() {
    const MahjongLayout& layout = og::mahjongLayoutFor(Difficulty::Medium, 1);
    MahjongBoard board(layout.slots, 11);
    const std::vector<std::pair<int, int>> solution = board.solution();
    for (std::size_t i = 0; i < 10; ++i) {
        assert(board.tap(solution[i].first) == MahjongBoard::Tap::Selected);
        assert(board.tap(solution[i].second) == MahjongBoard::Tap::Matched);
    }
    const int remaining = board.remaining();
    std::array<int, MahjongBoard::kGroupCount> before{};
    for (const MahjongBoard::Tile& t : board.tiles()) {
        if (!t.removed) {
            ++before.at(static_cast<std::size_t>(t.group));
        }
    }
    const std::vector<MahjongBoard::Tile> snapshot = board.tiles();
    assert(board.shuffle());
    assert(board.shuffles() == 1);
    assert(board.remaining() == remaining);
    std::array<int, MahjongBoard::kGroupCount> after{};
    for (const MahjongBoard::Tile& t : board.tiles()) {
        assert(t.slot == snapshot[static_cast<std::size_t>(t.id)].slot);
        assert(t.removed == snapshot[static_cast<std::size_t>(t.id)].removed);
        if (!t.removed) {
            ++after.at(static_cast<std::size_t>(t.group));
        }
    }
    assert(before == after);
    checkDeal(board);
    assert(board.undo()); // shuffle is undoable
    for (const MahjongBoard::Tile& t : board.tiles()) {
        assert(t.group == snapshot[static_cast<std::size_t>(t.id)].group);
    }
    assert(board.shuffle());
    replaySolution(board);
    assert(!board.shuffle()); // nothing left to shuffle
}

void testDeterminism() {
    const MahjongLayout& layout = og::mahjongLayoutFor(Difficulty::Hard, 2);
    const MahjongBoard a(layout.slots, og::mahjongLevelSeed(Difficulty::Hard, 2));
    const MahjongBoard b(layout.slots, og::mahjongLevelSeed(Difficulty::Hard, 2));
    const MahjongBoard c(layout.slots, og::mahjongLevelSeed(Difficulty::Hard, 3));
    bool differs = false;
    for (std::size_t i = 0; i < a.tiles().size(); ++i) {
        assert(a.tiles()[i].group == b.tiles()[i].group);
        differs = differs || a.tiles()[i].group != c.tiles()[i].group;
    }
    assert(differs);
    // Levels cycle through the tier's layouts.
    const auto count = static_cast<int>(og::mahjongLayouts(Difficulty::Easy).size());
    assert(&og::mahjongLayoutFor(Difficulty::Easy, 1) ==
           &og::mahjongLayoutFor(Difficulty::Easy, 1 + count));
    assert(og::mahjongLevelSeed(Difficulty::Easy, 1) !=
           og::mahjongLevelSeed(Difficulty::Medium, 1));
}

// An odd slot list drops its last slot; a tiny board still deals.
void testOddSlots() {
    const std::vector<Slot> slots{{0, 0, 0}, {2, 0, 0}, {4, 0, 0}};
    MahjongBoard board(slots, 1);
    assert(board.tileCount() == 2);
    replaySolution(board);
}

} // namespace

int main() {
    testFreeRule();
    testTapSemantics();
    testLayoutsAndDeals();
    testShuffle();
    testDeterminism();
    testOddSlots();
    std::puts("mahjong: all tests passed");
    return 0;
}
