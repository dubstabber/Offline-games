#include "games/tapmatch/TapMatchBoard.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace og {
namespace {

[[nodiscard]] int clampInt(int value, int lo, int hi) {
    return std::max(lo, std::min(value, hi));
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
    params_.holderBudget = clampInt(params_.holderBudget, kGroupSize, kHolderCapacity);
    params_.tileCount = std::max(kGroupSize, params_.tileCount);

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

void TapMatchBoard::placeTiles(std::mt19937_64& rng) {
    const int layers = params_.layers;
    const int maxOx = params_.gridWidth - kTileSpan;  // inclusive max footprint origin x
    const int maxOy = params_.gridHeight - kTileSpan; // inclusive max footprint origin y
    const double cx = static_cast<double>(maxOx) / 2.0;
    const double cy = static_cast<double>(maxOy) / 2.0;

    struct Candidate {
        int x = 0;
        int y = 0;
        double dist = 0.0;
    };

    // Per-layer footprint candidates on a step-2 lattice (so footprints on one
    // layer never overlap — the invariant that makes a peel order always exist),
    // closest-to-centre first for a compact pile. Alternating offsets shift
    // consecutive layers so higher ones interleave with and cover lower ones.
    std::vector<std::vector<Candidate>> layerCands(static_cast<std::size_t>(layers));
    long long weightSum = 0;
    for (int l = 0; l < layers; ++l) {
        weightSum += (2 * layers) - l;
        const int ox = l % 2;
        const int oy = (l / 2) % 2;
        std::vector<Candidate>& cands = layerCands.at(static_cast<std::size_t>(l));
        for (int x = ox; x <= maxOx; x += kTileSpan) {
            for (int y = oy; y <= maxOy; y += kTileSpan) {
                const double dx = static_cast<double>(x) - cx;
                const double dy = static_cast<double>(y) - cy;
                cands.push_back(Candidate{.x = x, .y = y, .dist = (dx * dx) + (dy * dy)});
            }
        }
        std::ranges::shuffle(cands, rng); // random tie-break for variety
        std::ranges::stable_sort(
            cands, [](const Candidate& a, const Candidate& b) { return a.dist < b.dist; });
    }

    // Distribute the requested tile count across layers by the pyramid weights,
    // capped by each layer's capacity, then hand out the flooring remainder to
    // the roomiest lower layers so the total lands exactly on tileCount.
    std::vector<int> count(static_cast<std::size_t>(layers), 0);
    int assigned = 0;
    for (int l = 0; l < layers; ++l) {
        const auto cap = static_cast<int>(layerCands.at(static_cast<std::size_t>(l)).size());
        const int want = clampInt(
            static_cast<int>((static_cast<long long>(params_.tileCount) * ((2 * layers) - l)) /
                             weightSum),
            0, cap);
        count.at(static_cast<std::size_t>(l)) = want;
        assigned += want;
    }
    for (int l = 0; l < layers && assigned < params_.tileCount; ++l) {
        const auto cap = static_cast<int>(layerCands.at(static_cast<std::size_t>(l)).size());
        const int add =
            std::min(cap - count.at(static_cast<std::size_t>(l)), params_.tileCount - assigned);
        count.at(static_cast<std::size_t>(l)) += add;
        assigned += add;
    }

    int nextId = 0;
    for (int l = 0; l < layers; ++l) {
        const std::vector<Candidate>& cands = layerCands.at(static_cast<std::size_t>(l));
        const int n = count.at(static_cast<std::size_t>(l));
        for (int i = 0; i < n; ++i) {
            const Candidate& c = cands.at(static_cast<std::size_t>(i));
            tiles_.push_back(
                Tile{.id = nextId, .icon = 0, .layer = l, .x = c.x, .y = c.y, .removed = false});
            ++nextId;
        }
    }

    // Keep the count a multiple of kGroupSize so every icon can close out (only
    // bites if capacity fell short of tileCount, which the params avoid).
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
        if (holderSize == kHolderCapacity && !completesTriple) {
            return false; // this tap would overflow the holder -> not a win
        }
        ++c;
        ++holderSize;
        if (completesTriple) {
            c = 0;
            holderSize -= kGroupSize;
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

    // Holder full and this tile can't complete a triple: there's no room for it,
    // so the tap loses. Board and holder are frozen at the moment of defeat.
    if (holderSize_ == kHolderCapacity && !completesTriple) {
        result_ = Result::Lost;
        return true;
    }

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

    if (remaining_ == 0) {
        result_ = Result::Won;
    }
    return true;
}

} // namespace og
