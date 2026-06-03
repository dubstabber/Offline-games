#include "games/minesweeper/MineSweeperSolver.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <random>
#include <utility>
#include <vector>

namespace og {
namespace {

// Largest frontier component (or end-game unknown set) we brute-force. 2^16 masks
// is cheap; bigger components are left undetermined, so a board that would need a
// heavier search is simply regenerated (it stays solvable by *simpler* logic).
constexpr int kEnumCap = 16;

// What the solver knows about a cell.
enum class Known : std::uint8_t { Unknown, Revealed, Mine };

// A revealed number imposes: exactly `mines` of its still-unknown neighbours
// `cells` are mines.
struct Constraint {
    std::vector<int> cells;
    int mines = 0;
};

// Union-find over cell indices, used to split the constrained frontier into
// independent components.
struct DisjointSet {
    std::vector<int> parent;

    explicit DisjointSet(int n) : parent(static_cast<std::size_t>(n)) {
        for (int i = 0; i < n; ++i) {
            parent.at(static_cast<std::size_t>(i)) = i;
        }
    }

    int find(int x) {
        while (parent.at(static_cast<std::size_t>(x)) != x) {
            const int grandparent =
                parent.at(static_cast<std::size_t>(parent.at(static_cast<std::size_t>(x))));
            parent.at(static_cast<std::size_t>(x)) = grandparent;
            x = grandparent;
        }
        return x;
    }

    void unite(int a, int b) { parent.at(static_cast<std::size_t>(find(a))) = find(b); }
};

// Simulates a player who only ever makes forced moves. Operates on the true mine
// layout: every deduction is implied by it, so the simulation never reveals a
// mine and any board it fully clears is genuinely solvable without guessing.
class Solver {
public:
    Solver(int width, int height, int mineCount, const std::vector<std::uint8_t>& layout)
        : w_(width), h_(height), n_(width * height), mineCount_(mineCount), mine_(layout),
          adj_(static_cast<std::size_t>(width * height), 0),
          status_(static_cast<std::size_t>(width * height), Known::Unknown) {
        computeAdjacency();
    }

    // Reveal the opening, then deduce to a fixpoint. True iff every safe cell ends
    // up revealed.
    bool solve(int r0, int c0) {
        const int start = index(r0, c0);
        if (isMine(start)) {
            return false;
        }
        reveal(start);
        while (simplePass() || endgameCountPass() || subsetPass() || enumEndgamePass() ||
               frontierEnumPass()) {
            // keep deducing
        }
        return solved();
    }

private:
    [[nodiscard]] int index(int r, int c) const { return (r * w_) + c; }
    [[nodiscard]] Known st(int i) const { return status_.at(static_cast<std::size_t>(i)); }
    [[nodiscard]] int adjAt(int i) const { return adj_.at(static_cast<std::size_t>(i)); }
    [[nodiscard]] bool isMine(int i) const { return mine_.at(static_cast<std::size_t>(i)) != 0; }

    template <class F> void forEachNeighbor(int i, const F& fn) const {
        const int r = i / w_;
        const int c = i % w_;
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0) {
                    continue;
                }
                const int nr = r + dr;
                const int nc = c + dc;
                if (nr >= 0 && nr < h_ && nc >= 0 && nc < w_) {
                    fn(index(nr, nc));
                }
            }
        }
    }

    void computeAdjacency() {
        for (int i = 0; i < n_; ++i) {
            if (isMine(i)) {
                continue;
            }
            int count = 0;
            forEachNeighbor(i, [&](int j) {
                if (isMine(j)) {
                    ++count;
                }
            });
            adj_.at(static_cast<std::size_t>(i)) = count;
        }
    }

    // Reveal a safe cell, flood-filling through zero-cells just like the board.
    void reveal(int start) {
        if (st(start) != Known::Unknown || isMine(start)) {
            return;
        }
        work_.clear();
        work_.push_back(start);
        while (!work_.empty()) {
            const int i = work_.back();
            work_.pop_back();
            if (st(i) != Known::Unknown) {
                continue;
            }
            status_.at(static_cast<std::size_t>(i)) = Known::Revealed;
            if (adjAt(i) == 0) {
                forEachNeighbor(i, [&](int j) {
                    if (st(j) == Known::Unknown && !isMine(j)) {
                        work_.push_back(j);
                    }
                });
            }
        }
    }

    void markMine(int i) {
        if (st(i) == Known::Unknown) {
            status_.at(static_cast<std::size_t>(i)) = Known::Mine;
            ++knownMines_;
        }
    }

    // Reveal (asMine=false) or flag (asMine=true) every still-unknown cell; returns
    // whether anything changed.
    bool forceCells(const std::vector<int>& cells, bool asMine) {
        bool progress = false;
        for (const int cell : cells) {
            if (st(cell) != Known::Unknown) {
                continue;
            }
            if (asMine) {
                markMine(cell);
            } else {
                reveal(cell);
            }
            progress = true;
        }
        return progress;
    }

    [[nodiscard]] std::vector<int> unknownCells() const {
        std::vector<int> cells;
        for (int i = 0; i < n_; ++i) {
            if (st(i) == Known::Unknown) {
                cells.push_back(i);
            }
        }
        return cells;
    }

    [[nodiscard]] bool solved() const {
        for (int i = 0; i < n_; ++i) {
            if (!isMine(i) && st(i) != Known::Revealed) {
                return false;
            }
        }
        return true;
    }

    // Single-constraint rule: a revealed number whose missing mines equal its
    // unknown neighbours marks them all mines; one with no missing mines reveals
    // them all.
    bool simplePass() {
        bool progress = false;
        for (int i = 0; i < n_; ++i) {
            if (st(i) != Known::Revealed) {
                continue;
            }
            int mines = 0;
            std::vector<int> unknown;
            forEachNeighbor(i, [&](int j) {
                if (st(j) == Known::Mine) {
                    ++mines;
                } else if (st(j) == Known::Unknown) {
                    unknown.push_back(j);
                }
            });
            if (unknown.empty()) {
                continue;
            }
            const int remaining = adjAt(i) - mines;
            if (remaining == 0) {
                progress = forceCells(unknown, false) || progress;
            } else if (std::cmp_equal(remaining, unknown.size())) {
                progress = forceCells(unknown, true) || progress;
            }
        }
        return progress;
    }

    // Whole-board count: if no mines remain, everything else is safe; if every
    // remaining unknown must be a mine, flag them all.
    bool endgameCountPass() {
        const std::vector<int> unknown = unknownCells();
        if (unknown.empty()) {
            return false;
        }
        const int remainingMines = mineCount_ - knownMines_;
        if (remainingMines == 0) {
            return forceCells(unknown, false);
        }
        if (std::cmp_equal(remainingMines, unknown.size())) {
            return forceCells(unknown, true);
        }
        return false;
    }

    [[nodiscard]] std::vector<Constraint> buildConstraints() const {
        std::vector<Constraint> constraints;
        for (int i = 0; i < n_; ++i) {
            if (st(i) != Known::Revealed) {
                continue;
            }
            Constraint c;
            int knownMines = 0;
            forEachNeighbor(i, [&](int j) {
                if (st(j) == Known::Unknown) {
                    c.cells.push_back(j);
                } else if (st(j) == Known::Mine) {
                    ++knownMines;
                }
            });
            if (c.cells.empty()) {
                continue;
            }
            c.mines = adjAt(i) - knownMines;
            constraints.push_back(std::move(c));
        }
        return constraints;
    }

    static bool isSubset(const std::vector<int>& a, const std::vector<int>& b) {
        return std::ranges::all_of(a,
                                   [&](int cell) { return std::ranges::find(b, cell) != b.end(); });
    }

    // The cells of `b` that are not in `a`.
    static std::vector<int> difference(const std::vector<int>& a, const std::vector<int>& b) {
        std::vector<int> diff;
        for (const int cell : b) {
            if (std::ranges::find(a, cell) == a.end()) {
                diff.push_back(cell);
            }
        }
        return diff;
    }

    // Pairwise overlap rule: when one constraint's cells are a subset of another's,
    // the extra cells carry the extra mines — forcing them all-mine or all-safe.
    bool subsetPass() {
        const std::vector<Constraint> cs = buildConstraints();
        for (std::size_t a = 0; a < cs.size(); ++a) {
            for (std::size_t b = 0; b < cs.size(); ++b) {
                const Constraint& ca = cs.at(a);
                const Constraint& cb = cs.at(b);
                if (a == b || ca.cells.size() >= cb.cells.size() || !isSubset(ca.cells, cb.cells)) {
                    continue;
                }
                const std::vector<int> diff = difference(ca.cells, cb.cells);
                const int dk = cb.mines - ca.mines;
                bool progress = false;
                if (dk == 0) {
                    progress = forceCells(diff, false);
                } else if (std::cmp_equal(dk, diff.size())) {
                    progress = forceCells(diff, true);
                }
                if (progress) {
                    return true; // constraints are now stale; rebuild next round
                }
            }
        }
        return false;
    }

    // Brute-force a small unknown set: enumerate every mine assignment satisfying
    // `cons` (constraint cell-masks + targets) and, if `totalMines >= 0`, the global
    // count. Cells that are a mine in every solution → mine; safe in every → reveal.
    bool applyEnumeration(const std::vector<int>& cells,
                          const std::vector<std::pair<unsigned, int>>& cons, int totalMines) {
        const int k = static_cast<int>(cells.size());
        if (k == 0 || k > kEnumCap) {
            return false;
        }
        unsigned andAcc = ~0U;
        unsigned orAcc = 0U;
        int valid = 0;
        const unsigned limit = 1U << static_cast<unsigned>(k);
        for (unsigned mask = 0; mask < limit; ++mask) {
            if (totalMines >= 0 && std::popcount(mask) != totalMines) {
                continue;
            }
            const bool ok = std::ranges::all_of(cons, [&](const auto& constraint) {
                return std::popcount(mask & constraint.first) == constraint.second;
            });
            if (!ok) {
                continue;
            }
            andAcc &= mask;
            orAcc |= mask;
            ++valid;
        }
        if (valid == 0) {
            return false;
        }
        std::vector<int> forcedMines;
        std::vector<int> forcedSafe;
        for (int p = 0; p < k; ++p) {
            const unsigned bit = 1U << static_cast<unsigned>(p);
            const int cell = cells.at(static_cast<std::size_t>(p));
            if ((andAcc & bit) != 0) {
                forcedMines.push_back(cell);
            } else if ((orAcc & bit) == 0) {
                forcedSafe.push_back(cell);
            }
        }
        const bool markedMines = forceCells(forcedMines, true);
        const bool revealedSafe = forceCells(forcedSafe, false);
        return markedMines || revealedSafe;
    }

    // Translate constraints over `cells` into position bitmasks within `cells`.
    [[nodiscard]] static std::vector<std::pair<unsigned, int>>
    maskConstraints(const std::vector<int>& cells, const std::vector<const Constraint*>& cons) {
        std::vector<std::pair<unsigned, int>> out;
        out.reserve(cons.size());
        for (const Constraint* c : cons) {
            unsigned mask = 0;
            for (const int cell : c->cells) {
                const auto it = std::ranges::find(cells, cell);
                if (it != cells.end()) {
                    mask |= 1U << static_cast<unsigned>(it - cells.begin());
                }
            }
            out.emplace_back(mask, c->mines);
        }
        return out;
    }

    // End-game CSP: once few unknowns remain, enumerate them all with the global
    // mine count, catching counting deductions the local rules miss.
    bool enumEndgamePass() {
        const std::vector<int> cells = unknownCells();
        if (cells.empty() || static_cast<int>(cells.size()) > kEnumCap) {
            return false;
        }
        const std::vector<Constraint> cs = buildConstraints();
        std::vector<const Constraint*> ptrs;
        ptrs.reserve(cs.size());
        for (const Constraint& c : cs) {
            ptrs.push_back(&c);
        }
        return applyEnumeration(cells, maskConstraints(cells, ptrs), mineCount_ - knownMines_);
    }

    // Mid-game local CSP: split the constrained frontier into independent
    // components and brute-force each small one (no global count needed).
    bool frontierEnumPass() {
        const std::vector<Constraint> cs = buildConstraints();
        if (cs.empty()) {
            return false;
        }
        DisjointSet ds(n_);
        for (const Constraint& c : cs) {
            for (std::size_t j = 1; j < c.cells.size(); ++j) {
                ds.unite(c.cells.at(0), c.cells.at(j));
            }
        }
        // Group constrained cells by component root.
        std::vector<std::uint8_t> seen(static_cast<std::size_t>(n_), 0);
        std::vector<std::pair<int, std::vector<int>>> comps;
        for (const Constraint& c : cs) {
            for (const int cell : c.cells) {
                if (seen.at(static_cast<std::size_t>(cell)) == 0) {
                    seen.at(static_cast<std::size_t>(cell)) = 1;
                    componentCells(comps, ds.find(cell)).push_back(cell);
                }
            }
        }
        for (const auto& [root, cells] : comps) {
            if (cells.empty() || static_cast<int>(cells.size()) > kEnumCap) {
                continue;
            }
            std::vector<const Constraint*> ptrs;
            for (const Constraint& c : cs) {
                if (ds.find(c.cells.at(0)) == root) {
                    ptrs.push_back(&c);
                }
            }
            if (applyEnumeration(cells, maskConstraints(cells, ptrs), -1)) {
                return true; // constraints stale after reveals/marks; rebuild
            }
        }
        return false;
    }

    // The cell list for component `root`, created on first use.
    static std::vector<int>& componentCells(std::vector<std::pair<int, std::vector<int>>>& comps,
                                            int root) {
        for (auto& [r, cells] : comps) {
            if (r == root) {
                return cells;
            }
        }
        comps.emplace_back(root, std::vector<int>{});
        return comps.back().second;
    }

    int w_;
    int h_;
    int n_;
    int mineCount_;
    const std::vector<std::uint8_t>& mine_; // NOLINT(*-avoid-const-or-ref-data-members)
    std::vector<int> adj_;
    std::vector<Known> status_;
    std::vector<int> work_;
    int knownMines_ = 0;
};

} // namespace

bool isMineLayoutSolvable(int width, int height, const std::vector<std::uint8_t>& layout, int r0,
                          int c0) {
    if (width <= 0 || height <= 0 || static_cast<int>(layout.size()) != width * height) {
        return false;
    }
    if (r0 < 0 || r0 >= height || c0 < 0 || c0 >= width) {
        return false;
    }
    const auto openIndex = (static_cast<std::size_t>(r0) * static_cast<std::size_t>(width)) +
                           static_cast<std::size_t>(c0);
    if (layout.at(openIndex) != 0) {
        return false;
    }
    const int mineCount =
        static_cast<int>(std::ranges::count_if(layout, [](std::uint8_t v) { return v != 0; }));
    Solver solver(width, height, mineCount, layout);
    return solver.solve(r0, c0);
}

std::vector<std::uint8_t> generateSolvableMines(int width, int height, int mineCount, int r0,
                                                int c0, std::uint32_t seed) {
    const int n = width * height;
    std::vector<std::uint8_t> layout(static_cast<std::size_t>(std::max(0, n)), 0);
    if (width <= 0 || height <= 0 || r0 < 0 || r0 >= height || c0 < 0 || c0 >= width) {
        return layout;
    }
    // Forbid the opening cell and its eight neighbours so the first tap floods.
    std::vector<std::uint8_t> forbidden(static_cast<std::size_t>(n), 0);
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            const int nr = r0 + dr;
            const int nc = c0 + dc;
            if (nr >= 0 && nr < height && nc >= 0 && nc < width) {
                forbidden.at((static_cast<std::size_t>(nr) * static_cast<std::size_t>(width)) +
                             static_cast<std::size_t>(nc)) = 1;
            }
        }
    }
    std::vector<int> candidates;
    for (int i = 0; i < n; ++i) {
        if (forbidden.at(static_cast<std::size_t>(i)) == 0) {
            candidates.push_back(i);
        }
    }
    const int mines = std::clamp(mineCount, 0, static_cast<int>(candidates.size()));

    std::mt19937 rng(seed);
    constexpr int kMaxAttempts = 3000;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        std::ranges::fill(layout, static_cast<std::uint8_t>(0));
        std::ranges::shuffle(candidates, rng);
        for (int m = 0; m < mines; ++m) {
            layout.at(static_cast<std::size_t>(candidates.at(static_cast<std::size_t>(m)))) = 1;
        }
        if (isMineLayoutSolvable(width, height, layout, r0, c0)) {
            return layout;
        }
    }
    return layout; // pathological miss: still playable, just maybe not no-guess
}

} // namespace og
