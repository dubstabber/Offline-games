#pragma once

#include <cstdint>
#include <random>
#include <span>
#include <utility>
#include <vector>

namespace og {

// Pure Mahjong solitaire logic: no SDL, no rendering, fully unit-testable. The
// rendering layer (MahjongScene) reads this and draws it.
//
// Tiles sit on stacked layers over a half-tile grid: every tile covers a
// kSpan x kSpan footprint at (x, y) on layer z, so neighbouring tiles may be
// offset by half a tile. A tile is *free* when nothing lies on top of it and
// at least one of its long sides (left or right) has no tile touching it.
// Tap a free tile to select it, tap a second free tile of the same kind to
// remove both. Clear every tile to win. Boards are dealt in reverse so a
// clearing order always exists; if play still runs out of moves, shuffle()
// re-deals what is left, again solvably.
class MahjongBoard {
public:
    static constexpr int kSpan = 2; // footprint in half-tile units

    // Tile kinds. A group is what has to match: dots and bamboo and numbers
    // 1-9, the four winds, the three dragons, then flowers and seasons, whose
    // four tiles each show a different face yet all match one another.
    static constexpr int kDots = 0;
    static constexpr int kBamboo = 9;
    static constexpr int kNumbers = 18;
    static constexpr int kWinds = 27;
    static constexpr int kDragons = 31;
    static constexpr int kFlowers = 34;
    static constexpr int kSeasons = 35;
    static constexpr int kGroupCount = 36;
    static constexpr int kCopies = 4; // tiles per group in a full set

    // A position in a layout: top-left of the footprint in half-tile units
    // and the layer (0 = table).
    struct Slot {
        int x = 0;
        int y = 0;
        int z = 0;
        friend bool operator==(const Slot&, const Slot&) = default;
    };

    struct Tile {
        int id = 0;
        Slot slot;
        int group = 0;
        int face = 0; // flowers/seasons: which of the four; 0 otherwise
        bool removed = false;
    };

    enum class Tap : std::uint8_t {
        Selected,   // the tile is now the selection
        Deselected, // it was the selection; now nothing is
        Matched,    // it matched the selection: both removed
        Blocked,    // the tile is not free
        Ignored,    // no such live tile
    };

    // Deal a solvable board over `slots` (an odd trailing slot is dropped).
    MahjongBoard(std::span<const Slot> slots, std::uint32_t seed);

    [[nodiscard]] const std::vector<Tile>& tiles() const { return tiles_; }
    [[nodiscard]] int tileCount() const { return static_cast<int>(tiles_.size()); }
    [[nodiscard]] int remaining() const { return remaining_; }
    // Layout extent in half-tile units, and how many layers it has.
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] int layers() const { return layers_; }

    [[nodiscard]] bool isFree(int id) const;
    [[nodiscard]] bool matches(int a, int b) const; // same group, distinct live tiles
    [[nodiscard]] int selected() const { return selected_; }

    Tap tap(int id);
    void clearSelection() { selected_ = -1; }

    // A matchable pair of free tiles, or {-1, -1}; hasMoves() is its existence.
    [[nodiscard]] std::pair<int, int> hint() const;
    [[nodiscard]] bool hasMoves() const { return hint().first >= 0; }
    [[nodiscard]] bool isWon() const { return remaining_ == 0; }

    // Undo the last match (or shuffle). Returns false when there is nothing to undo.
    bool undo();
    [[nodiscard]] bool canUndo() const { return !history_.empty(); }
    [[nodiscard]] int shuffles() const { return shuffles_; }

    // Re-deal the remaining tiles' kinds over the remaining positions so a
    // clearing order exists again. Returns false when nothing is left.
    bool shuffle();

    // The dealer's own clearing order: pairs to tap, first tile first. Valid for
    // the board as dealt (and re-set by shuffle()); tests replay it.
    [[nodiscard]] const std::vector<std::pair<int, int>>& solution() const { return solution_; }

private:
    void computeNeighbours();
    void removePair(int a, int b);
    // Assign `pairPool` (one group per pair) over the live tiles in `ids`,
    // placing pairs in an order whose reverse clears them. False if no attempt
    // succeeded (the board is then dealt unsolvably as a last resort).
    bool dealInto(std::span<const int> ids, std::vector<int> pairPool);
    bool tryDeal(std::span<const int> ids, std::span<const int> pairPool,
                 std::vector<std::pair<int, int>>& order);
    [[nodiscard]] bool placeable(int id, const std::vector<std::uint8_t>& filled) const;
    [[nodiscard]] bool wouldTrap(int id, const std::vector<std::uint8_t>& filled, bool left) const;
    [[nodiscard]] bool staysFree(int id, int other, const std::vector<std::uint8_t>& filled) const;
    [[nodiscard]] int roll(int n);

    std::vector<Tile> tiles_;
    // Per tile: the live-or-not-yet-removed tiles touching it (layout adjacency,
    // fixed for the board's life; callers skip removed ones).
    std::vector<std::vector<int>> above_;
    std::vector<std::vector<int>> below_;
    std::vector<std::vector<int>> left_;
    std::vector<std::vector<int>> right_;
    std::vector<std::vector<Tile>> history_;
    std::vector<std::pair<int, int>> solution_;
    std::mt19937 rng_;
    int width_ = 0;
    int height_ = 0;
    int layers_ = 0;
    int remaining_ = 0;
    int selected_ = -1;
    int shuffles_ = 0;
};

} // namespace og
