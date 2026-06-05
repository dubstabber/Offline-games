#include "core/Easing.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

using og::ease::clampUnit;
using og::ease::easeInCubic;
using og::ease::easeOutBack;
using og::ease::easeOutCubic;
using og::ease::easeOutQuad;
using og::ease::lerp;

bool nearly(float a, float b) {
    return std::fabs(a - b) < 1e-5F;
}

// clampUnit pins values into [0, 1] and passes interior values through.
void testClampUnit() {
    assert(nearly(clampUnit(-0.5F), 0.0F));
    assert(nearly(clampUnit(0.0F), 0.0F));
    assert(nearly(clampUnit(0.4F), 0.4F));
    assert(nearly(clampUnit(1.0F), 1.0F));
    assert(nearly(clampUnit(2.5F), 1.0F));
}

// lerp hits both endpoints and the midpoint exactly.
void testLerp() {
    assert(nearly(lerp(2.0F, 10.0F, 0.0F), 2.0F));
    assert(nearly(lerp(2.0F, 10.0F, 1.0F), 10.0F));
    assert(nearly(lerp(2.0F, 10.0F, 0.5F), 6.0F));
    static_assert(lerp(0.0F, 4.0F, 0.25F) == 1.0F); // usable at compile time
}

// Every ease maps 0->0 and 1->1 (easeOutBack included).
void testEndpoints() {
    assert(nearly(easeOutCubic(0.0F), 0.0F));
    assert(nearly(easeOutCubic(1.0F), 1.0F));
    assert(nearly(easeInCubic(0.0F), 0.0F));
    assert(nearly(easeInCubic(1.0F), 1.0F));
    assert(nearly(easeOutQuad(0.0F), 0.0F));
    assert(nearly(easeOutQuad(1.0F), 1.0F));
    assert(nearly(easeOutBack(0.0F), 0.0F));
    assert(nearly(easeOutBack(1.0F), 1.0F));
}

// Known closed-form values at the midpoint.
void testKnownValues() {
    assert(nearly(easeInCubic(0.5F), 0.125F));  // 0.5^3
    assert(nearly(easeOutCubic(0.5F), 0.875F)); // 1 - 0.5^3
    assert(nearly(easeOutQuad(0.5F), 0.75F));   // 1 - 0.5^2
    static_assert(easeInCubic(0.0F) == 0.0F);
}

// Shape checks: ease-out is above the line early, ease-in below, and the "back"
// ease overshoots above 1 before settling.
void testShape() {
    assert(easeOutCubic(0.3F) > 0.3F);
    assert(easeInCubic(0.3F) < 0.3F);
    assert(easeOutBack(0.8F) > 1.0F);
}

} // namespace

int main() {
    testClampUnit();
    testLerp();
    testEndpoints();
    testKnownValues();
    testShape();
    std::puts("All Easing tests passed.");
    return 0;
}
