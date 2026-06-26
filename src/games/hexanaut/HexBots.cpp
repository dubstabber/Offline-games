#include "games/hexanaut/HexBots.hpp"

#include "games/hexanaut/BotController.hpp"

#include <algorithm>
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

[[nodiscard]] bool rivalTrailAt(const HexWorldView& v, PlayerId self, HexCoord c) {
    const PlayerId owner = v.trailOwnerAt(c);
    return owner != kNoTrail && owner != self;
}

[[nodiscard]] int onwardExitCount(const HexWorldView& v, PlayerId self, HexCoord from,
                                  HexDir heading) {
    int exits = 0;
    for (int d = 0; d < 6; ++d) {
        if (isSafe(v, self, from, heading, static_cast<HexDir>(d))) {
            ++exits;
        }
    }
    return exits;
}

[[nodiscard]] bool enemyThreatensTrail(const HexWorldView& v, PlayerId self, int radius) {
    return std::ranges::any_of(
        v.trailOf(self), [&](HexCoord trailCell) { return enemyNear(v, self, trailCell, radius); });
}

[[nodiscard]] int nearestRivalTrailDistance(const HexWorldView& v, PlayerId self, HexCoord from,
                                            int maxRadius) {
    int best = maxRadius + 1;
    for (int q = 0; q < v.playerCount(); ++q) {
        const auto rival = static_cast<PlayerId>(q);
        if (rival == self || !v.alive(rival)) {
            continue;
        }
        for (const HexCoord c : v.trailOf(rival)) {
            const int dist = hexDistance(from, c);
            best = std::min(dist, best);
        }
    }
    return best;
}

class SmartBot final : public BotController {
public:
    explicit SmartBot(std::uint32_t seed);

    HexDir decide(const HexWorldView& view, PlayerId self) override;

private:
    [[nodiscard]] bool shouldReturn(const HexWorldView& view, PlayerId self, HexCoord me,
                                    int trailLen) const;
    [[nodiscard]] static HexDir pickImmediateCut(const HexWorldView& view, PlayerId self,
                                                 HexCoord me, const std::array<HexDir, 6>& cands,
                                                 int nc);
    [[nodiscard]] static HexDir pickReturnDirection(const HexWorldView& view, PlayerId self,
                                                    HexCoord me, const std::array<HexDir, 6>& cands,
                                                    int nc);
    [[nodiscard]] HexDir pickExpansionDirection(const HexWorldView& view, PlayerId self,
                                                HexCoord me, HexDir heading,
                                                const std::array<HexDir, 6>& cands, int nc) const;
    [[nodiscard]] static int territoryScore(PlayerId owner, PlayerId self, int trailLen);
    [[nodiscard]] int loopIntentScore(const HexWorldView& view, PlayerId self, HexCoord me,
                                      HexCoord target, PlayerId owner, bool startingLoop,
                                      int trailLen, int ownDistNow) const;
    [[nodiscard]] static int rivalTrailPressureScore(const HexWorldView& view, PlayerId self,
                                                     HexCoord target, int rivalTrailNow,
                                                     int trailSearch);

    int expandTarget_ = 0;
    int threatRadius_ = 0;
    int attackRadius_ = 0;
    int minBankTrail_ = 0;
    int turnBias_ = 1;
};

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

SmartBot::SmartBot(std::uint32_t seed) {
    std::mt19937 rng(seed);
    expandTarget_ = std::uniform_int_distribution<int>(12, 22)(rng);
    threatRadius_ = std::uniform_int_distribution<int>(3, 5)(rng);
    attackRadius_ = std::uniform_int_distribution<int>(4, 6)(rng);
    minBankTrail_ = std::uniform_int_distribution<int>(5, 8)(rng);
    turnBias_ = std::uniform_int_distribution<int>(0, 1)(rng) == 0 ? 1 : -1;
}

bool SmartBot::shouldReturn(const HexWorldView& view, PlayerId self, HexCoord me,
                            int trailLen) const {
    if (trailLen <= 0) {
        return false;
    }
    constexpr int kMaxReturnSearch = 28;
    const int ownDist = view.distanceToOwn(me, self, kMaxReturnSearch);
    return trailLen >= expandTarget_ || enemyNear(view, self, me, threatRadius_) ||
           enemyThreatensTrail(view, self, threatRadius_) ||
           (trailLen >= minBankTrail_ && ownDist <= 2) || ownDist >= (expandTarget_ / 2);
}

HexDir SmartBot::pickImmediateCut(const HexWorldView& view, PlayerId self, HexCoord me,
                                  const std::array<HexDir, 6>& cands, int nc) {
    constexpr int kMaxReturnSearch = 28;
    HexDir best = HexDir::None;
    int bestScore = -100000;
    for (int i = 0; i < nc; ++i) {
        const HexDir cand = cands.at(static_cast<std::size_t>(i));
        const HexCoord target = neighbor(me, cand);
        if (!rivalTrailAt(view, self, target)) {
            continue;
        }
        const PlayerId victim = view.trailOwnerAt(target);
        int score = 500 + (static_cast<int>(view.trailOf(victim).size()) * 12);
        if (victim == 0) {
            score += 30; // the human's exposed trail is the most valuable pressure point
        }
        score -= view.distanceToOwn(target, self, kMaxReturnSearch) * 2;
        score += onwardExitCount(view, self, target, cand) * 4;
        if (score > bestScore) {
            bestScore = score;
            best = cand;
        }
    }
    return best;
}

HexDir SmartBot::pickReturnDirection(const HexWorldView& view, PlayerId self, HexCoord me,
                                     const std::array<HexDir, 6>& cands, int nc) {
    constexpr int kMaxReturnSearch = 28;
    HexDir best = cands.at(0);
    int bestScore = -100000;
    for (int i = 0; i < nc; ++i) {
        const HexDir cand = cands.at(static_cast<std::size_t>(i));
        const HexCoord target = neighbor(me, cand);
        int score = -view.distanceToOwn(target, self, kMaxReturnSearch) * 100;
        if (view.ownerAt(target) == self) {
            score += 240;
        }
        score += onwardExitCount(view, self, target, cand) * 8;
        if (score > bestScore) {
            bestScore = score;
            best = cand;
        }
    }
    return best;
}

int SmartBot::territoryScore(PlayerId owner, PlayerId self, int trailLen) {
    if (owner == self) {
        return trailLen > 0 ? 260 : -120;
    }
    if (owner == kNeutral) {
        return 110;
    }
    return 155;
}

int SmartBot::loopIntentScore(const HexWorldView& view, PlayerId self, HexCoord me, HexCoord target,
                              PlayerId owner, bool startingLoop, int trailLen,
                              int ownDistNow) const {
    const HexCoord home = view.homeOf(self);
    if (startingLoop) {
        int score = (hexDistance(target, home) - hexDistance(me, home)) * 50;
        if (owner != self) {
            score += 80;
        }
        return score;
    }
    if (trailLen <= 0) {
        return 0;
    }

    constexpr int kMaxReturnSearch = 28;
    int score = 0;
    const int targetOwnDist = view.distanceToOwn(target, self, kMaxReturnSearch);
    if (trailLen >= minBankTrail_ && targetOwnDist <= 2) {
        score += 220 - (targetOwnDist * 40);
    } else if (trailLen < (expandTarget_ / 2) && targetOwnDist > ownDistNow) {
        score += 35;
    }
    if (enemyNear(view, self, target, threatRadius_)) {
        score -= 100;
    }
    return score;
}

int SmartBot::rivalTrailPressureScore(const HexWorldView& view, PlayerId self, HexCoord target,
                                      int rivalTrailNow, int trailSearch) {
    const int rivalTrailDist = nearestRivalTrailDistance(view, self, target, trailSearch);
    if (rivalTrailDist > trailSearch) {
        return 0;
    }
    return ((rivalTrailNow - rivalTrailDist) * 60) + ((trailSearch - rivalTrailDist) * 12);
}

HexDir SmartBot::pickExpansionDirection(const HexWorldView& view, PlayerId self, HexCoord me,
                                        HexDir heading, const std::array<HexDir, 6>& cands,
                                        int nc) const {
    constexpr int kMaxReturnSearch = 28;
    const int trailLen = static_cast<int>(view.trailOf(self).size());
    const int ownDistNow = view.distanceToOwn(me, self, kMaxReturnSearch);
    const int trailSearch = attackRadius_ + 3;
    const int rivalTrailNow = nearestRivalTrailDistance(view, self, me, trailSearch);
    const bool startingLoop = view.ownerAt(me) == self && trailLen == 0;

    HexDir best = cands.at(0);
    int bestScore = -100000;
    for (int i = 0; i < nc; ++i) {
        const HexDir cand = cands.at(static_cast<std::size_t>(i));
        const HexCoord target = neighbor(me, cand);
        const PlayerId owner = view.ownerAt(target);
        int score = territoryScore(owner, self, trailLen);

        if (view.powerupAt(target) != 0 && owner != self) {
            score += 180;
        }

        score += loopIntentScore(view, self, me, target, owner, startingLoop, trailLen, ownDistNow);
        score += rivalTrailPressureScore(view, self, target, rivalTrailNow, trailSearch);
        score += onwardExitCount(view, self, target, cand) * 25;
        if (cand == heading) {
            score += 8;
        }
        if (cand == rotate(heading, turnBias_)) {
            score += 6;
        }

        if (score > bestScore) {
            bestScore = score;
            best = cand;
        }
    }
    return best;
}

HexDir SmartBot::decide(const HexWorldView& view, PlayerId self) {
    const HexCoord me = view.cellOf(self);
    const HexDir heading = view.headingOf(self);
    const int trailLen = static_cast<int>(view.trailOf(self).size());

    std::array<HexDir, 6> cands{};
    const int nc = collectSafe(view, self, me, heading, cands);
    if (nc == 0) {
        return heading;
    }

    const HexDir cut = pickImmediateCut(view, self, me, cands, nc);
    if (cut != HexDir::None) {
        return cut;
    }

    if (shouldReturn(view, self, me, trailLen)) {
        return pickReturnDirection(view, self, me, cands, nc);
    }

    return pickExpansionDirection(view, self, me, heading, cands, nc);
}

std::unique_ptr<BotController> makeBot(BotSkill skill, std::uint32_t seed) {
    switch (skill) {
    case BotSkill::Smart:
        return std::make_unique<SmartBot>(seed);
    case BotSkill::Basic:
        return std::make_unique<BasicBot>(seed);
    }
    return std::make_unique<BasicBot>(seed);
}

} // namespace og::hexanaut
