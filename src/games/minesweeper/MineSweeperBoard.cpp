#include "games/minesweeper/MineSweeperBoard.hpp"

#include "games/minesweeper/MineSweeperSolver.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace og {

MineSweeperBoard::MineSweeperBoard(int width, int height, int mineCount, std::uint32_t seed)
    : width_(std::max(1, width)), height_(std::max(1, height)),
      mineCount_(std::clamp(mineCount, 0, (std::max(1, width) * std::max(1, height)) - 1)),
      seed_(seed), cells_(static_cast<std::size_t>(width_ * height_)) {}

bool MineSweeperBoard::inBounds(int r, int c) const {
    return r >= 0 && r < height_ && c >= 0 && c < width_;
}

const MineSweeperBoard::Cell& MineSweeperBoard::at(int r, int c) const {
    return cells_.at(static_cast<std::size_t>(index(r, c)));
}

MineSweeperBoard::Cell& MineSweeperBoard::cell(int r, int c) {
    return cells_.at(static_cast<std::size_t>(index(r, c)));
}

void MineSweeperBoard::setLayout(const std::vector<std::uint8_t>& layout) {
    if (static_cast<int>(layout.size()) != width_ * height_) {
        return;
    }
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        cells_.at(i).mine = layout.at(i) != 0;
        cells_.at(i).revealed = false;
        cells_.at(i).exploded = false;
    }
    computeAdjacency();
    started_ = true;
}

void MineSweeperBoard::computeAdjacency() {
    for (int r = 0; r < height_; ++r) {
        for (int c = 0; c < width_; ++c) {
            int count = 0;
            forEachNeighbor(r, c, [&](int nr, int nc) {
                if (cell(nr, nc).mine) {
                    ++count;
                }
            });
            cell(r, c).adjacent = static_cast<std::uint8_t>(count);
        }
    }
}

void MineSweeperBoard::floodReveal(int r, int c) {
    std::vector<int> work;
    work.push_back(index(r, c));
    while (!work.empty()) {
        const int i = work.back();
        work.pop_back();
        Cell& cl = cells_.at(static_cast<std::size_t>(i));
        if (cl.revealed || cl.flagged || cl.mine) {
            continue;
        }
        cl.revealed = true;
        ++revealedSafe_;
        if (cl.adjacent == 0) {
            forEachNeighbor(i / width_, i % width_,
                            [&](int nr, int nc) { work.push_back(index(nr, nc)); });
        }
    }
}

void MineSweeperBoard::revealAllMines() {
    for (Cell& cl : cells_) {
        if (cl.mine) {
            cl.revealed = true;
        }
    }
}

void MineSweeperBoard::checkWin() {
    if (revealedSafe_ != (width_ * height_) - mineCount_) {
        return;
    }
    state_ = State::Won;
    // Auto-flag every remaining mine so the counter reads zero, like the original.
    for (Cell& cl : cells_) {
        if (cl.mine && !cl.flagged) {
            cl.flagged = true;
            ++flagCount_;
        }
    }
}

int MineSweeperBoard::adjacentFlags(int r, int c) const {
    int count = 0;
    forEachNeighbor(r, c, [&](int nr, int nc) {
        if (at(nr, nc).flagged) {
            ++count;
        }
    });
    return count;
}

void MineSweeperBoard::reveal(int r, int c) {
    if (state_ != State::Playing || !inBounds(r, c)) {
        return;
    }
    if (cell(r, c).revealed || cell(r, c).flagged) {
        return;
    }
    if (!started_) {
        setLayout(generateSolvableMines(width_, height_, mineCount_, r, c, seed_));
    }
    if (cell(r, c).mine) {
        cell(r, c).revealed = true;
        cell(r, c).exploded = true;
        state_ = State::Lost;
        revealAllMines();
        return;
    }
    floodReveal(r, c);
    checkWin();
}

void MineSweeperBoard::toggleFlag(int r, int c) {
    if (state_ != State::Playing || !inBounds(r, c)) {
        return;
    }
    Cell& cl = cell(r, c);
    if (cl.revealed) {
        return;
    }
    cl.flagged = !cl.flagged;
    flagCount_ += cl.flagged ? 1 : -1;
}

void MineSweeperBoard::chord(int r, int c) {
    if (state_ != State::Playing || !started_ || !inBounds(r, c)) {
        return;
    }
    const Cell& origin = cell(r, c);
    if (!origin.revealed || origin.adjacent == 0 || adjacentFlags(r, c) != origin.adjacent) {
        return;
    }
    // Collect first so revealing (which may flood) doesn't disturb the walk.
    std::vector<std::pair<int, int>> targets;
    forEachNeighbor(r, c, [&](int nr, int nc) {
        const Cell& nb = at(nr, nc);
        if (!nb.flagged && !nb.revealed) {
            targets.emplace_back(nr, nc);
        }
    });
    for (const auto& [nr, nc] : targets) {
        Cell& nb = cell(nr, nc);
        if (nb.revealed || nb.flagged) {
            continue; // already opened by an earlier neighbour's flood
        }
        if (nb.mine) {
            nb.revealed = true;
            nb.exploded = true;
            state_ = State::Lost;
            revealAllMines();
            return;
        }
        floodReveal(nr, nc);
    }
    checkWin();
}

} // namespace og
