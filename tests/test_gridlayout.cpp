#include "core/FixedTimestep.hpp"
#include "core/GridLayout.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

using og::grid::cellAt;
using og::grid::Fit;
using og::grid::fitCentered;

bool nearly(float a, float b) {
    return std::fabs(a - b) < 1e-4F;
}

// A wide, short area: the cell is limited by height, and the grid is centered in
// the leftover width.
void testFitHeightLimited() {
    const Fit f = fitCentered(10.0F, 20.0F, 600.0F, 400.0F, 4, 4, 200.0F);
    // byWidth=150, byHeight=100, cap=200 -> cell=100.
    assert(nearly(f.cellPx, 100.0F));
    // 400-wide grid centered in 600 -> originX = 10 + (600-400)/2 = 110.
    assert(nearly(f.originX, 110.0F));
    // 400-tall grid fills the 400 height -> originY = 20.
    assert(nearly(f.originY, 20.0F));
}

// A small grid in a big area: the cell is capped at maxCell and centered both ways.
void testFitMaxCellCap() {
    const Fit f = fitCentered(0.0F, 0.0F, 1000.0F, 1000.0F, 2, 2, 80.0F);
    assert(nearly(f.cellPx, 80.0F));
    assert(nearly(f.originX, (1000.0F - 160.0F) / 2.0F)); // 420
    assert(nearly(f.originY, 420.0F));
}

// An empty board (gridW/H < 1) is treated as 1x1 instead of dividing by zero.
void testFitGuardsZeroGrid() {
    const Fit f = fitCentered(0.0F, 0.0F, 100.0F, 100.0F, 0, 0, 1000.0F);
    assert(nearly(f.cellPx, 100.0F)); // min(100/1, 100/1, 1000)
}

// cellAt maps interior pixels to (col, row) and rejects pixels left of/above origin.
void testCellAt() {
    const Fit f = fitCentered(0.0F, 0.0F, 300.0F, 300.0F, 3, 3, 1000.0F); // cell=100, origin (0,0)
    int col = -1;
    int row = -1;
    assert(cellAt(f, 0.0F, 0.0F, col, row) && col == 0 && row == 0);
    assert(cellAt(f, 150.0F, 250.0F, col, row) && col == 1 && row == 2);
    assert(cellAt(f, 299.0F, 299.0F, col, row) && col == 2 && row == 2);
    assert(!cellAt(f, -1.0F, 50.0F, col, row));
    assert(!cellAt(f, 50.0F, -1.0F, col, row));
}

// advanceFixed runs one step per whole fixedDt and leaves the remainder in accum.
// All magnitudes are exact in binary float so the step count is unambiguous.
void testAdvanceFixed() {
    float accum = 0.0F;
    int calls = 0;
    const int steps = og::advanceFixed(accum, 0.625F, 0.25F, 10.0F, [&] { ++calls; });
    assert(steps == 2 && calls == 2); // 0.625 / 0.25 -> 2 steps
    assert(nearly(accum, 0.125F));    // 0.625 - 2*0.25 left over
}

// A huge frame delta is clamped to maxAccumDt so the sim can't spiral.
void testAdvanceFixedClamp() {
    float accum = 0.0F;
    int calls = 0;
    const int steps = og::advanceFixed(accum, 100.0F, 0.25F, 2.0F, [&] { ++calls; });
    assert(steps == 8 && calls == 8); // clamp to 2.0 -> 2.0 / 0.25 = 8
    assert(nearly(accum, 0.0F));
}

} // namespace

int main() {
    testFitHeightLimited();
    testFitMaxCellCap();
    testFitGuardsZeroGrid();
    testCellAt();
    testAdvanceFixed();
    testAdvanceFixedClamp();
    std::puts("All GridLayout tests passed.");
    return 0;
}
