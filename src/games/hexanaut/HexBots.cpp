#include "games/hexanaut/HexBots.hpp"

#include "games/hexanaut/BotController.hpp"
#include "games/hexanaut/HexTypes.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <vector>

namespace og::hexanaut {
namespace {

constexpr int kMinLoop = 2;           // smallest hexagon-loop radius worth planning
constexpr int kMaxWaypoints = 8;      // 5 corners + the closing cell, with room to spare
constexpr int kReturnSearch = 48;     // BFS depth bound when heading back to own land
constexpr int kHomeSearch = 24;       // BFS depth bound when sizing up a rival's way home
constexpr int kHuntSearchSlack = 6;   // extra BFS depth over huntRadius for the actual route
constexpr int kLostSlack = 4;         // a waypoint this far beyond the loop size = off plan
constexpr int kFarRival = 30;         // distance cap when looking for the nearest rival
constexpr float kApproachCos = 0.8F;  // heading within ~35° of a cell counts as coming for it
constexpr float kItemValue = 12.0F;   // plan value of enclosing a static map item
constexpr float kCutValue = 25.0F;    // plan value of a rival trail lying across the ring
constexpr float kExposureCost = 0.5F; // plan cost per step spent outside own land
constexpr float kApproachCost = 0.4F; // plan cost per step spent walking out through own land
constexpr float kStraightBonus = 6.0F;
constexpr float kSharpTurnCost = 4.0F;
constexpr float kBiasBonus = 2.0F;
constexpr float kPlanJitter = 2.0F;
constexpr float kNoScore = -std::numeric_limits<float>::infinity();

[[nodiscard]] HexDir rotate(HexDir h, int k) {
    return static_cast<HexDir>((((static_cast<int>(h) + k) % 6) + 6) % 6);
}

// Signed 60° steps from `from` to `to`: 0 straight, ±1 gentle, ±2 sharp, 3 reversal.
[[nodiscard]] int turnSteps(HexDir from, HexDir to) {
    const int d = (((static_cast<int>(to) - static_cast<int>(from)) % 6) + 6) % 6;
    return d > 3 ? d - 6 : d;
}

struct Cands {
    std::array<HexDir, 6> dirs{};
    int count = 0;

    [[nodiscard]] bool has(HexDir d) const {
        for (int i = 0; i < count; ++i) {
            if (dirs.at(static_cast<std::size_t>(i)) == d) {
                return true;
            }
        }
        return false;
    }
};

[[nodiscard]] bool ownTrailAt(const HexWorldView& v, PlayerId self, HexCoord c) {
    return v.inBounds(c) && v.trailOwnerAt(c) == self;
}

[[nodiscard]] bool rivalTrailAt(const HexWorldView& v, PlayerId self, HexCoord c) {
    const PlayerId owner = v.trailOwnerAt(c);
    return owner != kNoTrail && owner != self;
}

// Directions that won't kill me on the spot: no reversal, in bounds, not onto my
// own trail. A 120° turn also needs the cell on the inside of the turn clear —
// a free-moving avatar can't pivot in place, it sweeps through that cell first.
[[nodiscard]] Cands safeDirections(const HexWorldView& v, PlayerId self, HexCoord me,
                                   HexDir heading) {
    Cands out;
    for (int d = 0; d < 6; ++d) {
        const auto dir = static_cast<HexDir>(d);
        const int turn = turnSteps(heading, dir);
        if (turn == 3) {
            continue;
        }
        const HexCoord t = neighbor(me, dir);
        if (!v.inBounds(t) || v.trailOwnerAt(t) == self) {
            continue;
        }
        if ((turn == 2 || turn == -2) &&
            ownTrailAt(v, self, neighbor(me, rotate(heading, turn / 2)))) {
            continue;
        }
        out.dirs.at(static_cast<std::size_t>(out.count)) = dir;
        ++out.count;
    }
    return out;
}

[[nodiscard]] bool isRival(const HexWorldView& v, PlayerId self, PlayerId other) {
    return other != self && v.alive(other);
}

[[nodiscard]] int nearestRivalDistance(const HexWorldView& v, PlayerId self, HexCoord c) {
    int best = kFarRival;
    for (int q = 0; q < v.playerCount(); ++q) {
        const auto id = static_cast<PlayerId>(q);
        if (isRival(v, self, id)) {
            best = std::min(best, hexDistance(v.cellOf(id), c));
        }
    }
    return best;
}

// True if `rival` is already touching cell `c` or is headed roughly at it.
[[nodiscard]] bool approaching(const HexWorldView& v, PlayerId rival, HexCoord c) {
    const HexCoord from = v.cellOf(rival);
    if (hexDistance(from, c) <= 1) {
        return true;
    }
    const Vec2 d = axialToWorld(c, 1.0F) - axialToWorld(from, 1.0F);
    const float len = length(d);
    if (len <= 0.0F) {
        return true;
    }
    const Vec2 h = unitFromAngle(dirAngle(v.headingOf(rival)));
    return ((d.x * h.x) + (d.y * h.y)) / len >= kApproachCos;
}

// The planning bot. It plays in three modes:
//  - Loop: plan a whole hexagon loop out of own territory and back (scored by the
//    land it would enclose against how easily any rival could cut it), then walk
//    its corners.
//  - Hunt: a rival is out on a trail and I can reach it before they get home —
//    go cut it (a kill annexes their whole territory).
//  - Return: my trail is exposed to someone who can reach it in time; take the
//    shortest safe route back onto own land.
// Every decision re-checks threats and opportunities, so a loop is abandoned the
// moment it turns dangerous and a hunt is dropped the moment it can't succeed.
class SmartBot final : public BotController {
public:
    SmartBot(const BotProfile& profile, std::uint32_t seed);

    HexDir decide(const HexWorldView& view, PlayerId self) override;

private:
    enum class Mode : std::uint8_t { Loop, Hunt, Return };

    // A hexagon loop reduced to the corners still ahead plus the cell where it
    // re-enters own land (the trail closes there).
    struct Plan {
        std::array<HexCoord, kMaxWaypoints> waypoints{};
        int count = 0;
        float score = kNoScore;
    };

    void ensureScratch(const HexWorldView& view);
    void syncState(HexCoord me, int trailLen);
    [[nodiscard]] int remainingSteps(HexCoord me, int ownDist) const;
    void advanceWaypoints(HexCoord me);

    [[nodiscard]] static HexDir pickImmediateCut(const HexWorldView& view, PlayerId self,
                                                 HexCoord me, const Cands& cands);
    [[nodiscard]] bool trailThreatened(const HexWorldView& view, PlayerId self,
                                       int remaining) const;
    [[nodiscard]] bool findHunt(const HexWorldView& view, PlayerId self, HexCoord me);
    [[nodiscard]] HexDir followLoop(const HexWorldView& view, PlayerId self, HexCoord me,
                                    HexDir heading, const Cands& cands, int trailLen,
                                    HexDir ownDir);
    [[nodiscard]] bool planLoop(const HexWorldView& view, PlayerId self, HexCoord me,
                                HexDir heading);
    [[nodiscard]] bool evaluateLoop(const HexWorldView& view, PlayerId self, HexCoord me,
                                    HexDir heading, HexDir first, int bias, int radius, Plan& out);
    [[nodiscard]] bool walkRing(const HexWorldView& view, PlayerId self, HexCoord me, HexDir first,
                                int bias, int radius, Plan& out, int& exitStep, int& closeStep);
    [[nodiscard]] float enclosedValue(const HexWorldView& view, PlayerId self, HexCoord center,
                                      int radius);
    // Helpers of enclosedValue working on disc_, the local (2r+1)^2 map of the
    // disc: 0 open, 1 blocked (planned trail / own land), 2 leaked (outside).
    void seedLeaks(const HexWorldView& view, PlayerId self, HexCoord center, int radius);
    void floodLeaks(HexCoord center, int radius);
    [[nodiscard]] bool loopTooRisky(const HexWorldView& view, PlayerId self, HexCoord me,
                                    int radius, int closeStep, float& worstSlack) const;
    [[nodiscard]] float cellValue(const HexWorldView& view, PlayerId self, HexCoord c) const;
    [[nodiscard]] HexDir holdInside(const HexWorldView& view, PlayerId self, HexCoord me,
                                    HexDir heading, const Cands& cands);
    [[nodiscard]] static HexDir pickToward(const HexWorldView& view, PlayerId self, HexCoord me,
                                           HexDir heading, const Cands& cands, HexCoord target);
    [[nodiscard]] static HexDir wander(const HexWorldView& view, PlayerId self, HexCoord me,
                                       HexDir heading, const Cands& cands);

    // Breadth-first search over cells `mover` may enter (anything but its own
    // trail). Returns the depth of the first cell satisfying `goal` and, via
    // `out`, the first step toward it; -1 if none within `maxDepth`. When `cands`
    // is given the first ring is restricted to those (safe) directions.
    template <class Goal>
    [[nodiscard]] int bfsFirstStep(const HexWorldView& view, PlayerId mover, HexCoord from,
                                   const Cands* cands, int maxDepth, Goal goal, HexDir& out);
    // Enqueue `c` for the running search if it is enterable and unseen, tagging it
    // with the first step that leads there; true if `c` is the goal.
    template <class Goal>
    [[nodiscard]] bool tryVisit(const HexWorldView& view, PlayerId mover, HexCoord c,
                                std::uint8_t first, Goal goal);
    [[nodiscard]] std::size_t cellIndex(HexCoord c) const {
        return (static_cast<std::size_t>(c.r) * static_cast<std::size_t>(gridW_)) +
               static_cast<std::size_t>(c.q);
    }

    std::mt19937 rng_;
    int maxLoop_;
    float tolerance_;
    float riskWeight_;
    int huntRadius_;
    float huntMargin_;
    float rivalLandWeight_;
    float humanBounty_;
    int turnBias_ = 1; // +1 or -1: which way it prefers its loops to curl

    Mode mode_ = Mode::Loop;
    std::array<HexCoord, kMaxWaypoints> waypoints_{};
    int waypointCount_ = 0;
    int nextWaypoint_ = 0;
    HexCoord huntTarget_{};
    int huntDist_ = 0;
    HexCoord lastCell_{.q = -1000, .r = -1000};
    bool hadTrail_ = false;

    // Search scratch, sized to the board on first use.
    int gridW_ = 0;
    int gridH_ = 0;
    std::int32_t stampId_ = 0;
    std::vector<std::int32_t> stamp_;
    std::vector<std::uint8_t> firstDir_;
    std::vector<int> queue_;
    std::vector<HexCoord> walk_;     // exposed cells of the ring being evaluated
    std::vector<std::uint8_t> disc_; // local (2r+1)^2 map for the enclosure flood
    std::vector<HexCoord> flood_;    // stack for that flood
};

SmartBot::SmartBot(const BotProfile& profile, std::uint32_t seed)
    : rng_(seed), maxLoop_(profile.maxLoop), tolerance_(profile.tolerance),
      riskWeight_(profile.riskWeight), huntRadius_(profile.huntRadius),
      huntMargin_(profile.huntMargin), rivalLandWeight_(profile.rivalLandWeight),
      humanBounty_(profile.humanBounty) {
    // Per-bot variation: slightly bolder or shyer than the preset, and a preferred
    // curl direction so a difficulty's bots don't all loop the same way.
    maxLoop_ = std::max(kMinLoop + 1, maxLoop_ + std::uniform_int_distribution<int>(-1, 1)(rng_));
    tolerance_ *= std::uniform_real_distribution<float>(0.85F, 1.15F)(rng_);
    turnBias_ = std::uniform_int_distribution<int>(0, 1)(rng_) == 0 ? 1 : -1;
}

void SmartBot::ensureScratch(const HexWorldView& view) {
    if (gridW_ == view.gridW() && gridH_ == view.gridH()) {
        return;
    }
    gridW_ = view.gridW();
    gridH_ = view.gridH();
    const auto cells = static_cast<std::size_t>(gridW_) * static_cast<std::size_t>(gridH_);
    stamp_.assign(cells, 0);
    firstDir_.assign(cells, 0);
    stampId_ = 0;
}

void SmartBot::syncState(HexCoord me, int trailLen) {
    // A jump (respawn or teleport) or a closed trail ends whatever I was doing.
    const bool jumped = hexDistance(me, lastCell_) > 2;
    const bool closed = hadTrail_ && trailLen == 0;
    if (jumped || closed) {
        waypointCount_ = 0;
        nextWaypoint_ = 0;
        mode_ = Mode::Loop;
    }
    lastCell_ = me;
    hadTrail_ = trailLen > 0;
}

template <class Goal>
bool SmartBot::tryVisit(const HexWorldView& view, PlayerId mover, HexCoord c, std::uint8_t first,
                        Goal goal) {
    if (!view.inBounds(c) || view.trailOwnerAt(c) == mover) {
        return false;
    }
    const std::size_t i = cellIndex(c);
    if (stamp_.at(i) == stampId_) {
        return false;
    }
    if (goal(c)) {
        return true;
    }
    stamp_.at(i) = stampId_;
    firstDir_.at(i) = first;
    queue_.push_back(static_cast<int>(i));
    return false;
}

template <class Goal>
int SmartBot::bfsFirstStep(const HexWorldView& view, PlayerId mover, HexCoord from,
                           const Cands* cands, int maxDepth, Goal goal, HexDir& out) {
    ++stampId_;
    queue_.clear();
    stamp_.at(cellIndex(from)) = stampId_;
    for (int d = 0; d < 6; ++d) {
        const auto dir = static_cast<HexDir>(d);
        if (cands != nullptr && !cands->has(dir)) {
            continue;
        }
        if (tryVisit(view, mover, neighbor(from, dir), static_cast<std::uint8_t>(d), goal)) {
            out = dir;
            return 1;
        }
    }
    std::size_t head = 0;
    for (int depth = 2; depth <= maxDepth && head < queue_.size(); ++depth) {
        const std::size_t levelEnd = queue_.size();
        for (; head < levelEnd; ++head) {
            const int ci = queue_.at(head);
            const HexCoord c{.q = ci % gridW_, .r = ci / gridW_};
            const std::uint8_t first = firstDir_.at(static_cast<std::size_t>(ci));
            for (int d = 0; d < 6; ++d) {
                if (tryVisit(view, mover, neighbor(c, static_cast<HexDir>(d)), first, goal)) {
                    out = static_cast<HexDir>(first);
                    return depth;
                }
            }
        }
    }
    return -1;
}

HexDir SmartBot::decide(const HexWorldView& view, PlayerId self) {
    ensureScratch(view);
    const HexCoord me = view.cellOf(self);
    const HexDir heading = view.headingOf(self);
    const int trailLen = static_cast<int>(view.trailOf(self).size());
    syncState(me, trailLen);

    const Cands cands = safeDirections(view, self, me, heading);
    if (cands.count == 0) {
        return heading; // boxed in; wall deflection is the only hope
    }

    // A rival's trail right next to me is an instant kill (and their land is mine).
    if (const HexDir cut = pickImmediateCut(view, self, me, cands); cut != HexDir::None) {
        return cut;
    }

    // Shortest safe way back onto own land (only meaningful while carrying a trail).
    HexDir ownDir = HexDir::None;
    int ownDist = -1;
    if (trailLen > 0) {
        ownDist = bfsFirstStep(
            view, self, me, &cands, kReturnSearch,
            [&](HexCoord c) { return view.ownerAt(c) == self; }, ownDir);
    }
    if (trailLen > 0 && mode_ != Mode::Return &&
        trailThreatened(view, self, remainingSteps(me, ownDist))) {
        mode_ = Mode::Return;
    }
    if (mode_ != Mode::Return) {
        if (findHunt(view, self, me)) {
            if (mode_ != Mode::Hunt) {
                waypointCount_ = 0; // the loop plan is stale once I detour
            }
            mode_ = Mode::Hunt;
        } else if (mode_ == Mode::Hunt) {
            mode_ = trailLen > 0 ? Mode::Return : Mode::Loop;
        }
    }
    if (mode_ == Mode::Hunt) {
        HexDir dir = HexDir::None;
        huntDist_ = bfsFirstStep(
            view, self, me, &cands, huntRadius_ + kHuntSearchSlack,
            [&](HexCoord c) { return c == huntTarget_; }, dir);
        if (huntDist_ > 0) {
            return dir;
        }
        mode_ = trailLen > 0 ? Mode::Return : Mode::Loop; // no safe route to the trail
    }
    if (mode_ == Mode::Return) {
        return ownDir != HexDir::None ? ownDir : wander(view, self, me, heading, cands);
    }
    return followLoop(view, self, me, heading, cands, trailLen, ownDir);
}

int SmartBot::remainingSteps(HexCoord me, int ownDist) const {
    // Steps until my trail closes if I keep doing what I'm doing.
    const int home = ownDist < 0 ? kReturnSearch : ownDist;
    if (mode_ == Mode::Hunt) {
        return huntDist_ + home;
    }
    if (mode_ == Mode::Loop && waypointCount_ > 0) {
        int steps = hexDistance(me, waypoints_.at(static_cast<std::size_t>(nextWaypoint_)));
        for (int i = nextWaypoint_ + 1; i < waypointCount_; ++i) {
            steps += hexDistance(waypoints_.at(static_cast<std::size_t>(i - 1)),
                                 waypoints_.at(static_cast<std::size_t>(i)));
        }
        return steps;
    }
    return home;
}

HexDir SmartBot::pickImmediateCut(const HexWorldView& view, PlayerId self, HexCoord me,
                                  const Cands& cands) {
    HexDir best = HexDir::None;
    float bestScore = kNoScore;
    for (int i = 0; i < cands.count; ++i) {
        const HexDir cand = cands.dirs.at(static_cast<std::size_t>(i));
        const HexCoord target = neighbor(me, cand);
        if (!rivalTrailAt(view, self, target)) {
            continue;
        }
        const PlayerId victim = view.trailOwnerAt(target);
        // Their head cell is fine to take — unless they are about to swap into mine,
        // which would cut us both.
        if (target == view.cellOf(victim) && neighbor(target, view.headingOf(victim)) == me) {
            continue;
        }
        const float score = static_cast<float>(view.territoryCount(victim)) +
                            static_cast<float>(view.trailOf(victim).size()) +
                            (victim == 0 ? 30.0F : 0.0F);
        if (score > bestScore) {
            bestScore = score;
            best = cand;
        }
    }
    return best;
}

bool SmartBot::trailThreatened(const HexWorldView& view, PlayerId self, int remaining) const {
    // Can any rival reach a cell of my trail before I close it? A rival that is
    // merely *able* to gets my tolerance; one already heading for it gets none.
    const float closeT = static_cast<float>(remaining) * view.stepIntervalOf(self);
    const std::span<const HexCoord> trail = view.trailOf(self);
    for (int q = 0; q < view.playerCount(); ++q) {
        const auto rival = static_cast<PlayerId>(q);
        if (!isRival(view, self, rival)) {
            continue;
        }
        const float interval = view.stepIntervalOf(rival);
        const HexCoord rc = view.cellOf(rival);
        for (const HexCoord c : trail) {
            const float slack = closeT - (static_cast<float>(hexDistance(rc, c)) * interval);
            if (slack <= 0.0F) {
                continue;
            }
            if (slack > tolerance_ || approaching(view, rival, c)) {
                return true;
            }
        }
    }
    return false;
}

bool SmartBot::findHunt(const HexWorldView& view, PlayerId self, HexCoord me) {
    // A rival out on a trail dies if I touch any cell of it before they get home.
    // Commit when my time to the nearest trail cell beats their shortest way back
    // by the profile's margin; prefer big territories (they become mine).
    const float myInterval = view.stepIntervalOf(self);
    float bestScore = kNoScore;
    bool found = false;
    for (int q = 0; q < view.playerCount(); ++q) {
        const auto rival = static_cast<PlayerId>(q);
        if (!isRival(view, self, rival)) {
            continue;
        }
        const std::span<const HexCoord> trail = view.trailOf(rival);
        if (trail.size() < 2) {
            continue; // only their head cell: nothing to run down yet
        }
        HexCoord nearest{};
        int nearestDist = huntRadius_ + 1;
        for (const HexCoord c : trail.first(trail.size() - 1)) { // never aim at the head
            const int d = hexDistance(me, c);
            if (d < nearestDist) {
                nearestDist = d;
                nearest = c;
            }
        }
        if (nearestDist > huntRadius_) {
            continue;
        }
        HexDir unused = HexDir::None;
        int home = bfsFirstStep(
            view, rival, view.cellOf(rival), nullptr, kHomeSearch,
            [&](HexCoord c) { return view.ownerAt(c) == rival; }, unused);
        if (home < 0) {
            home = kHomeSearch + 1;
        }
        const float theirT = static_cast<float>(home) * view.stepIntervalOf(rival);
        const float myT = static_cast<float>(nearestDist) * myInterval;
        if (myT + huntMargin_ >= theirT) {
            continue;
        }
        const float score =
            ((theirT - myT) * 10.0F) + (static_cast<float>(view.territoryCount(rival)) * 0.25F) -
            (static_cast<float>(nearestDist) * 2.0F) + (rival == 0 ? humanBounty_ : 0.0F);
        if (score > bestScore) {
            bestScore = score;
            huntTarget_ = nearest;
            huntDist_ = nearestDist;
            found = true;
        }
    }
    return found;
}

void SmartBot::advanceWaypoints(HexCoord me) {
    // Corners count as reached from one cell away (free movement rounds them);
    // the closing cell must actually be entered. A corner I have drifted past
    // — the next one is already nearer than it is from the corner — is done too.
    while (nextWaypoint_ < waypointCount_) {
        const HexCoord wp = waypoints_.at(static_cast<std::size_t>(nextWaypoint_));
        const bool last = nextWaypoint_ + 1 >= waypointCount_;
        const int d = hexDistance(me, wp);
        bool reached = d <= (last ? 0 : 1);
        if (!reached && !last && d <= 2) {
            const HexCoord after = waypoints_.at(static_cast<std::size_t>(nextWaypoint_) + 1);
            reached = hexDistance(me, after) < hexDistance(wp, after);
        }
        if (!reached) {
            break;
        }
        ++nextWaypoint_;
    }
}

HexDir SmartBot::followLoop(const HexWorldView& view, PlayerId self, HexCoord me, HexDir heading,
                            const Cands& cands, int trailLen, HexDir ownDir) {
    if (waypointCount_ > 0) {
        advanceWaypoints(me);
        const bool exhausted = nextWaypoint_ >= waypointCount_;
        const bool lost =
            !exhausted && hexDistance(me, waypoints_.at(static_cast<std::size_t>(nextWaypoint_))) >
                              (2 * maxLoop_) + kLostSlack;
        if (exhausted || lost) {
            waypointCount_ = 0; // the plan is over: re-plan at home, or bank the trail
        }
    }
    if (waypointCount_ == 0) {
        if (trailLen > 0) {
            mode_ = Mode::Return;
            return ownDir != HexDir::None ? ownDir : wander(view, self, me, heading, cands);
        }
        if (!planLoop(view, self, me, heading)) {
            return holdInside(view, self, me, heading, cands);
        }
    }
    return pickToward(view, self, me, heading, cands,
                      waypoints_.at(static_cast<std::size_t>(nextWaypoint_)));
}

bool SmartBot::planLoop(const HexWorldView& view, PlayerId self, HexCoord me, HexDir heading) {
    // Try every hexagon loop that starts with a non-reversing leg: both curl
    // directions, every radius. Keep the best-scoring one.
    std::uniform_real_distribution<float> jitter(0.0F, kPlanJitter);
    Plan best;
    Plan cand;
    for (int d = 0; d < 6; ++d) {
        const auto first = static_cast<HexDir>(d);
        if (turnSteps(heading, first) == 3) {
            continue;
        }
        for (const int bias : {1, -1}) {
            for (int radius = kMinLoop; radius <= maxLoop_; ++radius) {
                if (!evaluateLoop(view, self, me, heading, first, bias, radius, cand)) {
                    continue;
                }
                cand.score += jitter(rng_);
                if (cand.score > best.score) {
                    best = cand;
                }
            }
        }
    }
    if (best.count == 0) {
        return false;
    }
    waypoints_ = best.waypoints;
    waypointCount_ = best.count;
    nextWaypoint_ = 0;
    return true;
}

bool SmartBot::walkRing(const HexWorldView& view, PlayerId self, HexCoord me, HexDir first,
                        int bias, int radius, Plan& out, int& exitStep, int& closeStep) {
    // Walk the ring from my cell: `radius` cells per leg, turning 60° between legs.
    // Record the corners ahead and the exposed cells (outside own land) until the
    // ring re-enters own territory, where the trail would close. Rejected if it
    // leaves the board, never leaves home, or never gets back.
    walk_.clear();
    out.count = 0;
    exitStep = 0;
    closeStep = 0;
    HexCoord pos = me;
    int step = 0;
    for (int leg = 0; leg < 6 && closeStep == 0; ++leg) {
        const HexDir d = rotate(first, bias * leg);
        for (int k = 0; k < radius; ++k) {
            pos = neighbor(pos, d);
            ++step;
            if (!view.inBounds(pos)) {
                return false;
            }
            const bool own = view.ownerAt(pos) == self;
            if (exitStep == 0) {
                if (own) {
                    continue;
                }
                exitStep = step;
            } else if (own) {
                closeStep = step;
                break;
            }
            walk_.push_back(pos);
        }
        if (closeStep == 0) {
            out.waypoints.at(static_cast<std::size_t>(out.count)) = pos; // a corner ahead
            ++out.count;
        }
    }
    if (exitStep == 0 || closeStep == 0) {
        return false;
    }
    out.waypoints.at(static_cast<std::size_t>(out.count)) = pos; // where the trail closes
    ++out.count;
    return true;
}

bool SmartBot::evaluateLoop(const HexWorldView& view, PlayerId self, HexCoord me, HexDir heading,
                            HexDir first, int bias, int radius, Plan& out) {
    int exitStep = 0;
    int closeStep = 0;
    if (!walkRing(view, self, me, first, bias, radius, out, exitStep, closeStep)) {
        return false;
    }
    float slack = 0.0F;
    if (loopTooRisky(view, self, me, radius, closeStep, slack)) {
        return false;
    }
    // Walking `radius` cells per leg with a fixed 60° turn traces the ring of the
    // hex disc of that radius whose centre sits one radius off to the turning
    // side of the first leg.
    const HexCoord center = advance(me, rotate(first, bias), radius);
    float value = enclosedValue(view, self, center, radius);
    for (const HexCoord c : walk_) {
        value += cellValue(view, self, c) + (rivalTrailAt(view, self, c) ? kCutValue : 0.0F);
    }
    float score = value - (riskWeight_ * slack) -
                  (kExposureCost * static_cast<float>(walk_.size())) -
                  (kApproachCost * static_cast<float>(exitStep - 1));
    const int turn = turnSteps(heading, first);
    if (turn == 0) {
        score += kStraightBonus;
    } else if (turn == 2 || turn == -2) {
        score -= kSharpTurnCost;
    }
    if (bias == turnBias_) {
        score += kBiasBonus;
    }
    out.score = score;
    return true;
}

bool SmartBot::loopTooRisky(const HexWorldView& view, PlayerId self, HexCoord me, int radius,
                            int closeStep, float& worstSlack) const {
    // Same test as trailThreatened, applied to the cells I would be exposed on:
    // a rival that could get to one of them before I close the loop (by more than
    // my tolerance, or at all if they are already heading there) vetoes the plan.
    const float closeT = static_cast<float>(closeStep) * view.stepIntervalOf(self);
    worstSlack = 0.0F;
    for (int q = 0; q < view.playerCount(); ++q) {
        const auto rival = static_cast<PlayerId>(q);
        if (!isRival(view, self, rival)) {
            continue;
        }
        const float interval = view.stepIntervalOf(rival);
        const HexCoord rc = view.cellOf(rival);
        // Every ring cell is within 2*radius of me: a rival farther than that plus
        // the time budget can't matter, so skip the per-cell scan for them.
        const int minDist = hexDistance(rc, me) - (2 * radius);
        if (static_cast<float>(minDist) * interval >= closeT) {
            continue;
        }
        for (const HexCoord c : walk_) {
            const float slack = closeT - (static_cast<float>(hexDistance(rc, c)) * interval);
            if (slack <= 0.0F) {
                continue;
            }
            if (slack > tolerance_ || approaching(view, rival, c)) {
                return true;
            }
            worstSlack = std::max(worstSlack, slack);
        }
    }
    return false;
}

float SmartBot::cellValue(const HexWorldView& view, PlayerId self, HexCoord c) const {
    const PlayerId owner = view.ownerAt(c);
    if (owner == self) {
        return 0.0F;
    }
    const float land = owner == kNeutral ? 1.0F : rivalLandWeight_;
    return land + (view.powerupAt(c) != 0 ? kItemValue : 0.0F);
}

constexpr std::uint8_t kDiscOpen = 0;
constexpr std::uint8_t kDiscBlocked = 1;
constexpr std::uint8_t kDiscLeaked = 2;

// Index into the local (2r+1)^2 disc map for a cell within `radius` of `center`.
[[nodiscard]] std::size_t discIndex(HexCoord c, HexCoord center, int radius) {
    const int span = (2 * radius) + 1;
    return (static_cast<std::size_t>(c.q - center.q + radius) * static_cast<std::size_t>(span)) +
           static_cast<std::size_t>(c.r - center.r + radius);
}

void SmartBot::seedLeaks(const HexWorldView& view, PlayerId self, HexCoord center, int radius) {
    // Own land and the board edge are walls; an un-walked ring cell that is
    // neither is a gap through which the outside pours in.
    flood_.clear();
    for (int dq = -radius; dq <= radius; ++dq) {
        for (int dr = -radius; dr <= radius; ++dr) {
            const HexCoord c{.q = center.q + dq, .r = center.r + dr};
            const int dist = hexDistance(center, c);
            if (dist > radius) {
                continue;
            }
            std::uint8_t& state = disc_.at(discIndex(c, center, radius));
            if (state == kDiscBlocked) {
                continue;
            }
            if (!view.inBounds(c) || view.ownerAt(c) == self) {
                state = kDiscBlocked;
            } else if (dist == radius) {
                state = kDiscLeaked;
                flood_.push_back(c);
            }
        }
    }
}

void SmartBot::floodLeaks(HexCoord center, int radius) {
    while (!flood_.empty()) {
        const HexCoord c = flood_.back();
        flood_.pop_back();
        for (int d = 0; d < 6; ++d) {
            const HexCoord nb = neighbor(c, static_cast<HexDir>(d));
            if (hexDistance(center, nb) > radius) {
                continue;
            }
            std::uint8_t& state = disc_.at(discIndex(nb, center, radius));
            if (state == kDiscOpen) {
                state = kDiscLeaked;
                flood_.push_back(nb);
            }
        }
    }
}

float SmartBot::enclosedValue(const HexWorldView& view, PlayerId self, HexCoord center,
                              int radius) {
    // What the walked part of the ring would actually enclose: flood "outside"
    // into the disc from every ring cell I would NOT walk (and that isn't own
    // land), through open cells; whatever the flood never reaches is captured.
    const int span = (2 * radius) + 1;
    disc_.assign(static_cast<std::size_t>(span) * static_cast<std::size_t>(span), kDiscOpen);
    for (const HexCoord c : walk_) {
        disc_.at(discIndex(c, center, radius)) = kDiscBlocked;
    }
    seedLeaks(view, self, center, radius);
    floodLeaks(center, radius);
    float value = 0.0F;
    for (int dq = -radius; dq <= radius; ++dq) {
        for (int dr = -radius; dr <= radius; ++dr) {
            const HexCoord c{.q = center.q + dq, .r = center.r + dr};
            if (hexDistance(center, c) <= radius &&
                disc_.at(discIndex(c, center, radius)) == kDiscOpen) {
                value += cellValue(view, self, c);
            }
        }
    }
    return value;
}

HexDir SmartBot::holdInside(const HexWorldView& view, PlayerId self, HexCoord me, HexDir heading,
                            const Cands& cands) {
    // No loop is worth starting from here. If the frontier is out of reach of any
    // loop, walk out to it; otherwise (a rival too close, or walled in) linger on
    // own land, keeping away from rivals, until the picture changes.
    HexDir toFrontier = HexDir::None;
    const int frontier = bfsFirstStep(
        view, self, me, &cands, kReturnSearch, [&](HexCoord c) { return view.ownerAt(c) != self; },
        toFrontier);
    if (frontier > maxLoop_ && toFrontier != HexDir::None) {
        return toFrontier;
    }
    HexDir best = cands.dirs.at(0);
    int bestKey = std::numeric_limits<int>::min();
    for (int i = 0; i < cands.count; ++i) {
        const HexDir cand = cands.dirs.at(static_cast<std::size_t>(i));
        const HexCoord t = neighbor(me, cand);
        int key = (view.ownerAt(t) == self ? 1000 : 0) + (nearestRivalDistance(view, self, t) * 10);
        if (cand == heading) {
            key += 3;
        }
        if (key > bestKey) {
            bestKey = key;
            best = cand;
        }
    }
    return best;
}

HexDir SmartBot::pickToward(const HexWorldView& view, PlayerId self, HexCoord me, HexDir heading,
                            const Cands& cands, HexCoord target) {
    // Greedy step toward `target`: shortest distance first, then the gentlest
    // turn, and never next to a rival's head if an equal move avoids it.
    HexDir best = cands.dirs.at(0);
    int bestKey = std::numeric_limits<int>::max();
    for (int i = 0; i < cands.count; ++i) {
        const HexDir cand = cands.dirs.at(static_cast<std::size_t>(i));
        const HexCoord t = neighbor(me, cand);
        int key = (hexDistance(t, target) * 10) + iabs(turnSteps(heading, cand));
        if (nearestRivalDistance(view, self, t) <= 1) {
            key += 5;
        }
        if (key < bestKey) {
            bestKey = key;
            best = cand;
        }
    }
    return best;
}

HexDir SmartBot::wander(const HexWorldView& view, PlayerId self, HexCoord me, HexDir heading,
                        const Cands& cands) {
    // Nothing to go back to (own land is gone): keep options open, stay away from
    // rivals, and wait for a trail to cut.
    HexDir best = cands.dirs.at(0);
    int bestKey = std::numeric_limits<int>::min();
    for (int i = 0; i < cands.count; ++i) {
        const HexDir cand = cands.dirs.at(static_cast<std::size_t>(i));
        const HexCoord t = neighbor(me, cand);
        int key = (safeDirections(view, self, t, cand).count * 3) +
                  (nearestRivalDistance(view, self, t) * 2);
        if (cand == heading) {
            key += 1;
        }
        if (key > bestKey) {
            bestKey = key;
            best = cand;
        }
    }
    return best;
}

} // namespace

std::unique_ptr<BotController> makeBot(BotSkill skill, std::uint32_t seed) {
    return std::make_unique<SmartBot>(profileFor(skill), seed);
}

} // namespace og::hexanaut
