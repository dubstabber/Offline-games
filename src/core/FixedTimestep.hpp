#pragma once

#include <algorithm> // std::min

namespace og {

// Drive a fixed-timestep simulation. Clamp the frame delta to `maxAccumDt` (so a
// hitch can't make the sim spiral), add it to `accum`, and invoke `step` once for
// each whole `fixedDt` that has accumulated, decrementing `accum` in place. The
// `step` callable runs the caller's sub-step (advance the world, snapshot state,
// check death, ...). Returns how many steps ran this call. Header-only and
// inlined; the stepping math is identical to the hand-rolled loops it replaces.
template <typename Step>
int advanceFixed(float& accum, float dtSeconds, float fixedDt, float maxAccumDt, Step step) {
    accum += std::min(dtSeconds, maxAccumDt);
    int steps = 0;
    while (accum >= fixedDt) {
        step();
        accum -= fixedDt;
        ++steps;
    }
    return steps;
}

} // namespace og
