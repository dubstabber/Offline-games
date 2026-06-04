#include "games/hexanaut/HexBots.hpp"

#include "games/hexanaut/BotController.hpp"

#include <array>
#include <cstddef>
#include <random>

namespace og::hexanaut {
namespace {

// A direction is safe if it doesn't reverse, leave the board, or step onto my own
// trail (which would be fatal). Stepping onto a rival's trail is allowed — it kills
// them.
[[nodiscard]] bool isSafe(const HexWorldView& v, PlayerId self, HexCoord me, HexDir heading,
                          HexDir d) {
    if (d == opposite(heading)) {
        return false;
    }
    const HexCoord t = neighbor(me, d);
    return v.inBounds(t) && v.trailOwnerAt(t) != self;
}

[[nodiscard]] int collectSafe(const HexWorldView& v, PlayerId self, HexCoord me, HexDir heading,
                              std::array<HexDir, 6>& out) {
    int nc = 0;
    for (int d = 0; d < 6; ++d) {
        const auto hd = static_cast<HexDir>(d);
        if (isSafe(v, self, me, heading, hd)) {
            out.at(static_cast<std::size_t>(nc++)) = hd;
        }
    }
    return nc;
}

[[nodiscard]] bool contains(const std::array<HexDir, 6>& cands, int nc, HexDir d) {
    for (int i = 0; i < nc; ++i) {
        if (cands.at(static_cast<std::size_t>(i)) == d) {
            return true;
        }
    }
    return false;
}

// The safe candidate that most de-/increases the hex distance to `goal`.
[[nodiscard]] HexDir pickToward(HexCoord me, const std::array<HexDir, 6>& cands, int nc,
                                HexCoord goal, bool nearer) {
    HexDir best = cands.at(0);
    int bestDist = hexDistance(neighbor(me, best), goal);
    for (int i = 1; i < nc; ++i) {
        const HexDir cand = cands.at(static_cast<std::size_t>(i));
        const int dist = hexDistance(neighbor(me, cand), goal);
        if ((nearer && dist < bestDist) || (!nearer && dist > bestDist)) {
            bestDist = dist;
            best = cand;
        }
    }
    return best;
}

[[nodiscard]] bool enemyNear(const HexWorldView& v, PlayerId self, HexCoord me, int radius) {
    for (int q = 0; q < v.playerCount(); ++q) {
        const auto qq = static_cast<PlayerId>(q);
        if (qq == self || !v.alive(qq)) {
            continue;
        }
        if (hexDistance(v.cellOf(qq), me) <= radius) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] HexDir rotate(HexDir h, int k) {
    return static_cast<HexDir>((((static_cast<int>(h) + k) % 6) + 6) % 6);
}

} // namespace

BasicBot::BasicBot(std::uint32_t seed)
    : rng_(seed), legLength_(std::uniform_int_distribution<int>(2, 4)(rng_)),
      expandTarget_(std::uniform_int_distribution<int>(14, 26)(rng_)),
      threatRadius_(std::uniform_int_distribution<int>(2, 4)(rng_)),
      turnBias_(std::uniform_int_distribution<int>(0, 1)(rng_) == 0 ? 1 : -1) {}

HexDir BasicBot::decide(const HexWorldView& view, PlayerId self) {
    const HexCoord me = view.cellOf(self);
    const HexDir heading = view.headingOf(self);
    const HexCoord home = view.homeOf(self);
    const int trailLen = static_cast<int>(view.trailOf(self).size());

    std::array<HexDir, 6> cands{};
    const int nc = collectSafe(view, self, me, heading, cands);
    if (nc == 0) {
        return heading; // boxed in; the simulation's wall deflection is our only hope
    }

    // Standing on my own land with no trail: head outward to start a new loop.
    if (view.ownerAt(me) == self && trailLen == 0) {
        sinceTurn_ = 0;
        return pickToward(me, cands, nc, home, /*nearer=*/false);
    }
    // Exposed with a rival nearby, or the loop is long enough: dive home to bank it.
    if ((trailLen > 0 && enemyNear(view, self, me, threatRadius_)) || trailLen >= expandTarget_) {
        return pickToward(me, cands, nc, home, /*nearer=*/true);
    }

    // Carve the loop: straight legs, turning by the bias between them.
    if (sinceTurn_ < legLength_ && contains(cands, nc, heading)) {
        ++sinceTurn_;
        return heading;
    }
    sinceTurn_ = 0;
    if (contains(cands, nc, rotate(heading, turnBias_))) {
        return rotate(heading, turnBias_);
    }
    if (contains(cands, nc, rotate(heading, 2 * turnBias_))) {
        return rotate(heading, 2 * turnBias_);
    }
    if (contains(cands, nc, heading)) {
        return heading;
    }
    return cands.at(0);
}

std::unique_ptr<BotController> makeBot(BotSkill skill, std::uint32_t seed) {
    // Smart is not built yet (Phase E); fall back to Basic so the difficulty table
    // can already name it without breaking.
    (void)skill;
    return std::make_unique<BasicBot>(seed);
}

} // namespace og::hexanaut
