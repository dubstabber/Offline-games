#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace og {

// Pure game logic for Tap Match (a "triple tile" game): no SDL, no rendering,
// fully unit-testable. The matching TapMatchScene reads this and draws it.
//
// Tiles are laid out across stacked layers on a fine integer grid; each tile is
// a kTileSpan x kTileSpan footprint and a higher layer can overlap (cover) the
// tiles below it. A tile is *accessible* (tappable) only while no higher-layer
// tile overlaps its footprint. Tapping an accessible tile moves its icon into a
// 7-slot holder; three of the same icon clear together. Clear the whole board
// to win; fill the holder with no triple to clear and you lose.
//
// Every board is generated guaranteed-solvable: there is always a tap order that
// clears it without the holder ever exceeding 7 (see the .cpp). With no power-
// ups, that guarantee is what makes a perfect player always able to win.
class TapMatchBoard {
public:
    static constexpr int kHolderCapacity = 7;
    static constexpr int kMaxIcons = 20;
    static constexpr int kGroupSize = 3;
    static constexpr int kTileSpan = 2; // footprint is kTileSpan x kTileSpan fine cells

    enum class Result : std::uint8_t { Playing, Won, Lost };

    // One tile. (x, y) is the top-left of its kTileSpan x kTileSpan footprint in
    // fine grid cells; layer is the z-order (higher paints/​covers on top).
    // id == index into tiles().
    struct Tile {
        int id = 0;
        int icon = 0;
        int layer = 0;
        int x = 0;
        int y = 0;
        bool removed = false;
    };

    // Knobs that shape a generated board. Difficulty maps to one of these in the
    // scene. tileCount must be a multiple of kGroupSize and iconVariety in
    // [1, kMaxIcons]; holderBudget is the largest holder fill the generator's own
    // solution is allowed to use (<= kHolderCapacity).
    struct GenParams {
        int iconVariety = 6;
        int layers = 3;
        int tileCount = 36;
        int holderBudget = 7;
        int gridWidth = 9;
        int gridHeight = 11;
        int clusters = 1; // separate tile piles spread across the board (1..4)
    };

    TapMatchBoard(const GenParams& params, std::uint64_t seed);

    [[nodiscard]] const std::vector<Tile>& tiles() const { return tiles_; }
    [[nodiscard]] int iconVariety() const { return params_.iconVariety; }
    [[nodiscard]] int gridWidth() const { return params_.gridWidth; }
    [[nodiscard]] int gridHeight() const { return params_.gridHeight; }
    [[nodiscard]] int remaining() const { return remaining_; }
    [[nodiscard]] int holderSize() const { return holderSize_; }
    // Icons currently held, grouped so equal icons sit next to each other.
    [[nodiscard]] const std::vector<int>& holder() const { return holder_; }
    [[nodiscard]] Result result() const { return result_; }
    [[nodiscard]] bool isOver() const { return result_ != Result::Playing; }

    // True iff the tile exists, is not removed, and no higher-layer tile covers it.
    [[nodiscard]] bool isAccessible(int id) const;

    // A removal order that clears the whole board with holderSize never above
    // kHolderCapacity. Built during generation; exposed for tests / an optional
    // hint. Playing this order is a guaranteed win.
    [[nodiscard]] const std::vector<int>& solutionOrder() const { return solution_; }

    // Tap a tile by id. Returns false (and changes nothing) if the tile is
    // invalid, already removed, covered, or the game is over. Otherwise it moves
    // the tile's icon into the holder, resolves a triple if one completes, and
    // updates result() (Won when the board is clear, Lost when the holder fills).
    bool tapTile(int id);

private:
    [[nodiscard]] static bool footprintsOverlap(const Tile& a, const Tile& b);
    [[nodiscard]] Tile& tileAt(int id) { return tiles_.at(static_cast<std::size_t>(id)); }
    [[nodiscard]] const Tile& tileAt(int id) const {
        return tiles_.at(static_cast<std::size_t>(id));
    }

    void placeTiles(std::mt19937_64& rng);
    // Tiles per (cluster, layer): split tileCount across clusters, then each
    // cluster's share across layers by a lower-heavy pyramid weight.
    [[nodiscard]] std::vector<std::vector<int>> clusterLayerCounts(int clusters, int layers) const;
    void buildCoverGraph();
    void computeRemovalOrder(std::mt19937_64& rng);
    void assignIcons(std::mt19937_64& rng);
    void assignConsecutiveTriples();
    [[nodiscard]] bool simulateSolutionWins() const;

    GenParams params_;
    std::vector<Tile> tiles_;
    std::vector<std::vector<int>> coversBelow_; // tiles each tile sits on top of
    std::vector<int> coveringCount_;            // # of higher, non-removed coverers
    std::array<std::uint8_t, kMaxIcons> heldCount_{};
    std::vector<int> holder_;   // icons currently held, grouped by icon
    std::vector<int> solution_; // guaranteed-win removal order
    int remaining_ = 0;
    int holderSize_ = 0;
    Result result_ = Result::Playing;
};

} // namespace og
