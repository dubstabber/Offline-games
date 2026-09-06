#pragma once

#include "games/hexanaut/BotController.hpp"

#include <cstdint>
#include <memory>

namespace og::hexanaut {

// Personality knobs for the planning bot — one preset per BotSkill (profileFor).
// The seed adds a little per-bot variation on top, so one difficulty's bots don't
// all play identically.
struct BotProfile {
    int maxLoop;           // largest hexagon-loop radius it will plan (min is 2)
    float tolerance;       // seconds a rival may beat it to its trail by before it worries
    float riskWeight;      // plan-score cost per second of that slack
    int huntRadius;        // max hexes it will detour to cut a rival's exposed trail
    float huntMargin;      // seconds it must beat the rival home by before committing
    float rivalLandWeight; // value of enclosing one rival-owned cell (neutral = 1)
    float humanBounty;     // extra hunt priority for the human's trail
};

[[nodiscard]] constexpr BotProfile profileFor(BotSkill skill) {
    switch (skill) {
    case BotSkill::Cautious:
        return {.maxLoop = 5,
                .tolerance = 0.25F,
                .riskWeight = 12.0F,
                .huntRadius = 2,
                .huntMargin = 0.6F,
                .rivalLandWeight = 1.1F,
                .humanBounty = 0.0F};
    case BotSkill::Ruthless:
        return {.maxLoop = 9,
                .tolerance = 1.3F,
                .riskWeight = 4.0F,
                .huntRadius = 14,
                .huntMargin = 0.1F,
                .rivalLandWeight = 1.4F,
                .humanBounty = 30.0F};
    case BotSkill::Smart:
        break;
    }
    return {.maxLoop = 7,
            .tolerance = 0.7F,
            .riskWeight = 7.0F,
            .huntRadius = 8,
            .huntMargin = 0.3F,
            .rivalLandWeight = 1.25F,
            .humanBounty = 10.0F};
}

[[nodiscard]] std::unique_ptr<BotController> makeBot(BotSkill skill, std::uint32_t seed);

} // namespace og::hexanaut
