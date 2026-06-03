#include "games/tapmatch/TapMatchBoard.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace og {
namespace {

[[nodiscard]] int clampInt(int value, int lo, int hi) {
    return std::max(lo, std::min(value, hi));
}

// A footprint origin on the per-layer step-2 lattice.
struct GridPoint {
    int x = 0;
    int y = 0;
};

// All lattice origins for a layer (parity ox/oy). Two tiles on this lattice
// never overlap, so a whole layer is overlap-free regardless of clustering.
[[nodiscard]] std::vector<GridPoint> latticePoints(int ox, int oy, int maxOx, int maxOy) {
    std::vector<GridPoint> points;
    for (int x = ox; x <= maxOx; x += TapMatchBoard::kTileSpan) {
        for (int y = oy; y <= maxOy; y += TapMatchBoard::kTileSpan) {
            points.push_back(GridPoint{.x = x, .y = y});
        }
    }
    return points;
}

// The `need` unused points closest to (cx, cy), random among equal distances —
// so a cluster grows as a compact pile around its centre.
[[nodiscard]] std::vector<std::size_t> nearestUnusedIndices(const std::vector<GridPoint>& points,
                                                            const std::vector<char>& used,
                                                            double cx, double cy, int need,
                                                            std::mt19937_64& rng) {
    std::vector<std::size_t> idx;
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (used.at(i) == 0) {
            idx.push_back(i);
        }
    }
    auto dist2 = [&](std::size_t i) {
        const double dx = static_cast<double>(points.at(i).x) - cx;
        const double dy = static_cast<double>(points.at(i).y) - cy;
        return (dx * dx) + (dy * dy);
    };
    std::ranges::shuffle(idx, rng);
    std::ranges::stable_sort(idx,
                             [&](std::size_t a, std::size_t b) { return dist2(a) < dist2(b); });
    const auto keep = static_cast<std::size_t>(std::max(0, need));
    if (idx.size() > keep) {
        idx.resize(keep);
    }
    return idx;
}

// Icons bucketed by their partial count in the generator's virtual holder, plus
// the closing cost (extra tiles still needed to finish every open group).
struct IconBuckets {
    std::vector<int> ones;  // count == 1
    std::vector<int> twos;  // count == 2
    std::vector<int> frees; // count == 0
    int closingCost = 0;
    int openCount = 0;
};

[[nodiscard]] IconBuckets bucketIcons(const std::array<int, TapMatchBoard::kMaxIcons>& counts,
                                      int iconCount) {
    IconBuckets buckets;
    for (int i = 0; i < iconCount; ++i) {
        const int c = counts.at(static_cast<std::size_t>(i));
        if (c == 0) {
            buckets.frees.push_back(i);
        } else if (c == 1) {
            buckets.ones.push_back(i);
            buckets.closingCost += TapMatchBoard::kGroupSize - 1;
        } else {
            buckets.twos.push_back(i);
            buckets.closingCost += TapMatchBoard::kGroupSize - 2;
        }
    }
    buckets.openCount = static_cast<int>(buckets.ones.size() + buckets.twos.size());
    return buckets;
}

// Pick the icon for the next slot in the peel order so that playing the peel
// order keeps the holder <= budget and still closes every group:
//  OPEN  a fresh icon only while an all-singletons holder stays <= budget-1 (so a
//        group can always be advanced -> no deadlock) and there is room to close
//        everyone afterwards;
//  CONT  advances a single toward a pair (spreads a triple's three tiles apart);
//  CLOSE completes a pair (always safe — it frees a slot).
[[nodiscard]] int chooseAssignmentIcon(const IconBuckets& buckets, int holderSize, int slotsAfter,
                                       int budget, int targetOpen, std::mt19937_64& rng) {
    auto pick = [&rng](const std::vector<int>& bucket) {
        return bucket.at(std::uniform_int_distribution<std::size_t>(0, bucket.size() - 1)(rng));
    };
    std::uniform_real_distribution<double> coin(0.0, 1.0);

    const bool canOpen = !buckets.frees.empty() && holderSize <= budget - 2 &&
                         (buckets.closingCost + 2) <= slotsAfter;
    const bool canCont = !buckets.ones.empty() && (holderSize + 1) <= budget;
    const bool canClose = !buckets.twos.empty();

    if (canOpen && buckets.openCount < targetOpen && coin(rng) < 0.7) {
        return pick(buckets.frees);
    }
    if (canCont && (buckets.twos.empty() || coin(rng) < 0.6)) {
        return pick(buckets.ones);
    }
    if (canClose) {
        return pick(buckets.twos);
    }
    if (canCont) {
        return pick(buckets.ones);
    }
    // Nothing else is feasible, so open a fresh group. The invariants guarantee
    // `frees` is non-empty whenever we reach here.
    return pick(buckets.frees);
}

} // namespace

TapMatchBoard::TapMatchBoard(const GenParams& params, std::uint64_t seed) : params_(params) {
    // Sanitize params so the generation invariants below always hold.
    params_.iconVariety = clampInt(params_.iconVariety, 1, kMaxIcons);
    params_.layers = std::max(1, params_.layers);
    params_.gridWidth = std::max(kTileSpan + 1, params_.gridWidth);
    params_.gridHeight = std::max(kTileSpan + 1, params_.gridHeight);
    // Cap below capacity: a solvable peel order must keep a slot free to complete
    // its next triple, so it never parks tiles all the way to kHolderCapacity (that
    // now loses). The 7th slot only ever fills transiently mid-triple.
    params_.holderBudget = clampInt(params_.holderBudget, kGroupSize, kHolderCapacity - 1);
    params_.tileCount = std::max(kGroupSize, params_.tileCount);
    params_.clusters = clampInt(params_.clusters, 1, 4);

    std::mt19937_64 rng(seed);
    placeTiles(rng);          // geometry: layered, overlapping footprints
    buildCoverGraph();        // who covers whom
    computeRemovalOrder(rng); // a peel order that is always playable
    assignIcons(rng);         // colour it so that peel order also wins
    if (!simulateSolutionWins()) {
        // Defensive: the assignment above is solvable by construction, but never
        // ship an unsolvable board. Consecutive triples along the peel order are
        // trivially clearable (the holder never exceeds kGroupSize).
        assignConsecutiveTriples();
    }
}

bool TapMatchBoard::footprintsOverlap(const Tile& a, const Tile& b) {
    return a.x < b.x + kTileSpan && b.x < a.x + kTileSpan && a.y < b.y + kTileSpan &&
           b.y < a.y + kTileSpan;
}

std::vector<std::vector<int>> TapMatchBoard::clusterLayerCounts(int clusters, int layers) const {
    // Very base-heavy pyramid (weight = (layers - l)^2): a wide bottom layer with
    // only a couple of raised tiles on top, so a cluster reads as a tidy grid of
    // distinct tiles (with a few covering it) rather than a dense ball.
    long long weightSum = 0;
    for (int l = 0; l < layers; ++l) {
        weightSum += static_cast<long long>(layers - l) * (layers - l);
    }
    std::vector<std::vector<int>> count(static_cast<std::size_t>(clusters),
                                        std::vector<int>(static_cast<std::size_t>(layers), 0));
    for (int c = 0; c < clusters; ++c) {
        const int share =
            (params_.tileCount / clusters) + ((c < (params_.tileCount % clusters)) ? 1 : 0);
        std::vector<int>& layerCount = count.at(static_cast<std::size_t>(c));
        int assigned = 0;
        for (int l = 0; l < layers; ++l) {
            const long long weight = static_cast<long long>(layers - l) * (layers - l);
            const int want = static_cast<int>((static_cast<long long>(share) * weight) / weightSum);
            layerCount.at(static_cast<std::size_t>(l)) = want;
            assigned += want;
        }
        // Hand the flooring remainder to the upper layers (top first) so they are
        // never left empty.
        for (int l = layers - 1; l >= 0 && assigned < share; --l) {
            ++layerCount.at(static_cast<std::size_t>(l));
            ++assigned;
        }
    }
    return count;
}

void TapMatchBoard::placeTiles(std::mt19937_64& rng) {
    const int layers = params_.layers;
    const int clusters = params_.clusters;
    const int maxOx = params_.gridWidth - kTileSpan;  // inclusive max footprint origin x
    const int maxOy = params_.gridHeight - kTileSpan; // inclusive max footprint origin y

    // Cluster centres in a cols x rows meta-grid across the board (origin coords),
    // like the original's spread-out tile piles.
    const int cols = std::min(clusters, 2);
    const int rows = (clusters + cols - 1) / cols;
    std::vector<double> centerX(static_cast<std::size_t>(clusters));
    std::vector<double> centerY(static_cast<std::size_t>(clusters));
    for (int c = 0; c < clusters; ++c) {
        const int metaCol = c % cols;
        const int metaRow = c / cols;
        centerX.at(static_cast<std::size_t>(c)) =
            (static_cast<double>(metaCol) + 0.5) * static_cast<double>(maxOx + 1) / cols;
        centerY.at(static_cast<std::size_t>(c)) =
            (static_cast<double>(metaRow) + 0.5) * static_cast<double>(maxOy + 1) / rows;
    }

    const std::vector<std::vector<int>> count = clusterLayerCounts(clusters, layers);

    // Lay down each layer; within a layer, hand each cluster the lattice points
    // nearest its centre. Alternating per-layer parities make higher layers
    // interleave with and cover the ones below (so each cluster is a little
    // staircase pile). The lattice guarantees no two same-layer tiles overlap.
    int nextId = 0;
    for (int l = 0; l < layers; ++l) {
        const std::vector<GridPoint> points = latticePoints(l % 2, (l / 2) % 2, maxOx, maxOy);
        std::vector<char> used(points.size(), 0);
        for (int c = 0; c < clusters; ++c) {
            const std::vector<std::size_t> chosen = nearestUnusedIndices(
                points, used, centerX.at(static_cast<std::size_t>(c)),
                centerY.at(static_cast<std::size_t>(c)),
                count.at(static_cast<std::size_t>(c)).at(static_cast<std::size_t>(l)), rng);
            for (const std::size_t i : chosen) {
                used.at(i) = 1;
                const GridPoint p = points.at(i);
                tiles_.push_back(Tile{
                    .id = nextId, .icon = 0, .layer = l, .x = p.x, .y = p.y, .removed = false});
                ++nextId;
            }
        }
    }

    // Keep the count a multiple of kGroupSize so every icon can close out.
    while (!tiles_.empty() && (tiles_.size() % kGroupSize) != 0) {
        tiles_.pop_back();
    }
    remaining_ = static_cast<int>(tiles_.size());
}

void TapMatchBoard::buildCoverGraph() {
    const std::size_t n = tiles_.size();
    coversBelow_.assign(n, {});
    coveringCount_.assign(n, 0);
    for (std::size_t a = 0; a < n; ++a) {
        for (std::size_t b = 0; b < n; ++b) {
            if (a == b) {
                continue;
            }
            const Tile& ta = tiles_.at(a);
            const Tile& tb = tiles_.at(b);
            if (ta.layer > tb.layer && footprintsOverlap(ta, tb)) {
                coversBelow_.at(a).push_back(tb.id); // a sits on top of b
                ++coveringCount_.at(b);
            }
        }
    }
}

void TapMatchBoard::computeRemovalOrder(std::mt19937_64& rng) {
    const std::size_t n = tiles_.size();
    std::vector<int> cover = coveringCount_; // working copy
    std::vector<int> accessible;
    for (std::size_t i = 0; i < n; ++i) {
        if (cover.at(i) == 0) {
            accessible.push_back(static_cast<int>(i));
        }
    }
    solution_.clear();
    solution_.reserve(n);
    while (!accessible.empty()) {
        std::uniform_int_distribution<std::size_t> pick(0, accessible.size() - 1);
        const std::size_t k = pick(rng);
        const int id = accessible.at(k);
        accessible.at(k) = accessible.back();
        accessible.pop_back();
        solution_.push_back(id);
        for (const int below : coversBelow_.at(static_cast<std::size_t>(id))) {
            if (--cover.at(static_cast<std::size_t>(below)) == 0) {
                accessible.push_back(below);
            }
        }
    }
}

void TapMatchBoard::assignIcons(std::mt19937_64& rng) {
    const int total = static_cast<int>(solution_.size());
    const int budget = params_.holderBudget;
    const int targetOpen = std::min(params_.iconVariety, budget - 1);

    std::array<int, kMaxIcons> counts{}; // virtual holder: partial count per icon (0..2)
    int holderSize = 0;

    for (int k = 0; k < total; ++k) {
        const int slotsAfter = total - k - 1;
        const IconBuckets buckets = bucketIcons(counts, params_.iconVariety);
        const int chosen =
            chooseAssignmentIcon(buckets, holderSize, slotsAfter, budget, targetOpen, rng);

        tileAt(solution_.at(static_cast<std::size_t>(k))).icon = chosen;
        int& c = counts.at(static_cast<std::size_t>(chosen));
        ++c;
        ++holderSize;
        if (c == kGroupSize) {
            c = 0;
            holderSize -= kGroupSize;
        }
    }
}

void TapMatchBoard::assignConsecutiveTriples() {
    const int total = static_cast<int>(solution_.size());
    const int iconCount = params_.iconVariety;
    for (int k = 0; k < total; ++k) {
        tileAt(solution_.at(static_cast<std::size_t>(k))).icon = (k / kGroupSize) % iconCount;
    }
}

bool TapMatchBoard::simulateSolutionWins() const {
    if (solution_.size() != tiles_.size()) {
        return false;
    }
    std::array<int, kMaxIcons> held{};
    int holderSize = 0;
    for (const int id : solution_) {
        const int icon = tileAt(id).icon;
        if (icon < 0 || icon >= kMaxIcons) {
            return false;
        }
        int& c = held.at(static_cast<std::size_t>(icon));
        const bool completesTriple = c == (kGroupSize - 1);
        ++c;
        ++holderSize;
        if (completesTriple) {
            c = 0;
            holderSize -= kGroupSize;
        } else if (holderSize == kHolderCapacity) {
            return false; // a non-clearing tap fills the holder -> this order loses
        }
    }
    return holderSize == 0; // the whole board cleared
}

bool TapMatchBoard::isAccessible(int id) const {
    if (id < 0 || static_cast<std::size_t>(id) >= tiles_.size()) {
        return false;
    }
    const Tile& t = tileAt(id);
    return !t.removed && coveringCount_.at(static_cast<std::size_t>(id)) == 0;
}

bool TapMatchBoard::tapTile(int id) {
    if (result_ != Result::Playing) {
        return false;
    }
    if (id < 0 || static_cast<std::size_t>(id) >= tiles_.size()) {
        return false;
    }
    Tile& t = tileAt(id);
    if (t.removed || coveringCount_.at(static_cast<std::size_t>(id)) != 0) {
        return false;
    }

    const int icon = t.icon;
    auto& held = heldCount_.at(static_cast<std::size_t>(icon));
    const bool completesTriple = held == (kGroupSize - 1); // a third of this icon

    // Leave the board; uncover the tiles this one was sitting on.
    t.removed = true;
    --remaining_;
    for (const int below : coversBelow_.at(static_cast<std::size_t>(id))) {
        --coveringCount_.at(static_cast<std::size_t>(below));
    }

    // Into the holder, kept grouped so a triple is contiguous; clear on three.
    ++held;
    ++holderSize_;
    holder_.insert(std::ranges::find(holder_, icon), icon);
    if (completesTriple) {
        held = 0;
        holderSize_ -= kGroupSize;
        std::erase(holder_, icon);
    }

    // Win when the board is clear; otherwise lose the instant the holder fills up
    // with no triple to clear it — there is no room for another tile, so you can't
    // tap your way out of a full holder (a triple can only complete while a slot is
    // still free, filling to 7 then clearing back down).
    if (remaining_ == 0) {
        result_ = Result::Won;
    } else if (holderSize_ == kHolderCapacity) {
        result_ = Result::Lost;
    }
    return true;
}

} // namespace og
