#pragma once

#include "games/hexanaut/BotController.hpp"
#include "games/hexanaut/HexTypes.hpp"

#include <cstdint>
#include <memory>
#include <random>

namespace og::hexanaut {

// v1 bot: leave home and carve a rough loop by running short straight legs with a
// fixed turn bias, then dive back to its own territory to capture the enclosed
// area. It avoids stepping on its own trail and bails home early when a rival gets
// close. Plays only off HexWorldView, so a stronger SmartBot can replace it with
// zero engine change (see the plan's Phase E).
class BasicBot : public BotController {
public:
    explicit BasicBot(std::uint32_t seed);

    HexDir decide(const HexWorldView& view, PlayerId self) override;

private:
    std::mt19937 rng_;
    int legLength_;     // straight cells per leg before a turn
    int expandTarget_;  // trail length backstop that forces a return
    int threatRadius_;  // a rival this close to me triggers a retreat
    int turnBias_;      // +1 or -1: which way the loop curls
    int sinceTurn_ = 0; // cells since the last turn
};

[[nodiscard]] std::unique_ptr<BotController> makeBot(BotSkill skill, std::uint32_t seed);

} // namespace og::hexanaut
