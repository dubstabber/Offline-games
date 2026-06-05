#include "games/hexanaut/HexWorld.hpp"

#include "games/hexanaut/BotController.hpp"
#include "games/hexanaut/HexBots.hpp"
#include "games/hexanaut/HexConfig.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <deque>
#include <numbers>
#include <unordered_set>
#include <utility>
#include <vector>

namespace og::hexanaut {
namespace {

// Display names for bots, in the spirit of the original's leaderboard.
constexpr std::array<const char*, 12> kBotNames{"kING",    "MANSEL", "Ukraine", "Pixel",
                                                "Comet",   "Vortex", "Zephyr",  "Nova",
                                                "Strider", "Blaze",  "Echo",    "Drift"};

[[nodiscard]] const char* botName(int botIndex) {
    return kBotNames.at(static_cast<std::size_t>(botIndex - 1) % kBotNames.size());
}

} // namespace

HexWorld::HexWorld(int difficultyIndex, std::uint32_t seed)
    : grid_(config::paramsFor(difficultyIndex).gridW, config::paramsFor(difficultyIndex).gridH),
      rng_(seed), totalCells_(grid_.cellCount()) {
    const config::DifficultyParams params = config::paramsFor(difficultyIndex);
    visited_.assign(static_cast<std::size_t>(totalCells_), 0);
    powerupInterval_ = params.powerupInterval;
    maxPowerups_ = params.maxPowerups;

    Player human;
    human.id = 0;
    human.isBot = false;
    human.name = "YOU";
    human.stepInterval = params.playerStepInterval;
    human.baseStepInterval = params.playerStepInterval;
    players_.push_back(std::move(human));
    bots_.push_back(nullptr);

    const HexCoord center{grid_.width() / 2, grid_.height() / 2};
    spawnHome(players_.at(0), center, config::kHomeRadius);

    for (int i = 1; i <= params.botCount; ++i) {
        Player bot;
        bot.id = static_cast<PlayerId>(i);
        bot.isBot = true;
        bot.name = botName(i);
        bot.stepInterval = params.botStepInterval;
        bot.baseStepInterval = params.botStepInterval;
        players_.push_back(std::move(bot));
        bots_.push_back(makeBot(params.botSkill, static_cast<std::uint32_t>(rng_())));
        spawnHome(players_.back(), findSpawn(config::kHomeRadius + 1), config::kHomeRadius);
    }

    generateShooters(params.shooterCount); // static items, placed once after all homes
}

void HexWorld::spawnHome(Player& p, HexCoord center, int radius) {
    p.cell = center;
    p.fromCell = center;
    p.home = center;
    p.pos = axialToWorld(center, config::kHexSize);
    p.angle = dirAngle(HexDir::N);
    p.desiredAngle = p.angle;
    p.heading = HexDir::N;
    p.desiredDir = HexDir::None;
    p.alive = true;
    p.trail.clear();
    p.stepInterval = p.baseStepInterval; // a respawn drops any active power-up
    p.speedTimer = 0.0F;
    p.visionTimer = 0.0F;
    for (int q = center.q - radius; q <= center.q + radius; ++q) {
        for (int r = center.r - radius; r <= center.r + radius; ++r) {
            const HexCoord h{q, r};
            if (grid_.contains(h) && hexDistance(center, h) <= radius) {
                setOwner(h, p.id);
            }
        }
    }
}

void HexWorld::setOwner(HexCoord c, PlayerId newOwner) {
    Cell& cell = grid_.at(c);
    if (cell.owner == newOwner) {
        return;
    }
    if (cell.owner != kNeutral) {
        players_.at(cell.owner).territoryCount -= 1;
    }
    cell.owner = newOwner;
    if (newOwner != kNeutral) {
        players_.at(newOwner).territoryCount += 1;
    }
}

void HexWorld::setPlayerDesiredAngle(float angleRad) {
    players_.front().desiredAngle = angleRad;
}

HexDir HexWorld::deflectHeading(HexCoord cell, HexDir heading) const {
    // Prefer the current heading, then the smallest turn that stays in bounds, so
    // an entity hitting a wall slides along it instead of dying.
    constexpr std::array<int, 6> kOrder{0, +1, -1, +2, -2, 3};
    const int base = static_cast<int>(heading);
    for (const int off : kOrder) {
        const auto d = static_cast<HexDir>((((base + off) % 6) + 6) % 6);
        if (grid_.contains(neighbor(cell, d))) {
            return d;
        }
    }
    return heading;
}

void HexWorld::decideBots() {
    const HexWorldView view(*this);
    for (Player& p : players_) {
        if (p.isBot && p.alive && bots_.at(static_cast<std::size_t>(p.id))) {
            p.desiredDir = bots_.at(static_cast<std::size_t>(p.id))->decide(view, p.id);
            if (p.desiredDir != HexDir::None) {
                p.desiredAngle = dirAngle(p.desiredDir); // bots steer toward their chosen axis
            }
        }
    }
}

void HexWorld::step() {
    decideBots();
    decayEffects();
    maybeSpawnPowerup();
    resolveTick(integrateMotion(), true);
    updateShooters(); // capture with lasers after movement/territory settle
}

void HexWorld::decayEffects() {
    for (Player& p : players_) {
        if (!p.alive) {
            continue;
        }
        if (p.speedTimer > 0.0F) {
            p.speedTimer -= config::kFixedDt;
            if (p.speedTimer <= 0.0F) {
                p.speedTimer = 0.0F;
                p.stepInterval = p.baseStepInterval;
            }
        }
        if (p.visionTimer > 0.0F) {
            p.visionTimer = std::max(0.0F, p.visionTimer - config::kFixedDt);
        }
    }
}

void HexWorld::maybeSpawnPowerup() {
    if (powerupInterval_ <= 0.0F) {
        return;
    }
    powerupAccum_ += config::kFixedDt;
    if (powerupAccum_ < powerupInterval_) {
        return;
    }
    powerupAccum_ = 0.0F;
    if (activePowerups_ >= maxPowerups_) {
        return;
    }
    std::uniform_int_distribution<int> qd(1, grid_.width() - 2);
    std::uniform_int_distribution<int> rd(1, grid_.height() - 2);
    for (int attempt = 0; attempt < 60; ++attempt) {
        const HexCoord c{qd(rng_), rd(rng_)};
        Cell& cell = grid_.at(c);
        if (cell.owner == kNeutral && cell.trailOwner == kNoTrail && cell.powerup == 0) {
            const PowerUp type = (rng_() % 2U == 0U) ? PowerUp::Speed : PowerUp::Vision;
            cell.powerup = static_cast<std::uint8_t>(type);
            ++activePowerups_;
            return;
        }
    }
}

void HexWorld::applyPowerup(Player& p, PowerUp type) {
    switch (type) {
    case PowerUp::Speed:
        p.stepInterval = p.baseStepInterval * config::kSpeedFactor;
        p.speedTimer = config::kSpeedDuration;
        break;
    case PowerUp::Vision:
        p.visionTimer = config::kVisionDuration;
        break;
    case PowerUp::Shooter: // not a collectible buff; handled by updateShooters
    case PowerUp::None:
        break;
    }
}

void HexWorld::generateShooters(int count) {
    // All shooters are placed up front on open ground (an un-captured shooter does
    // nothing, so each sits as a contestable prize until a player's territory
    // reaches it). Static for the whole match — they never move or respawn.
    std::uniform_int_distribution<int> qd(2, grid_.width() - 3);
    std::uniform_int_distribution<int> rd(2, grid_.height() - 3);
    for (int i = 0; i < count; ++i) {
        for (int attempt = 0; attempt < 80; ++attempt) {
            const HexCoord c{qd(rng_), rd(rng_)};
            Cell& cell = grid_.at(c);
            if (cell.owner == kNeutral && cell.trailOwner == kNoTrail && cell.powerup == 0) {
                cell.powerup = static_cast<std::uint8_t>(PowerUp::Shooter);
                shooters_.push_back(Shooter{.cell = c});
                break;
            }
        }
    }
}

bool HexWorld::nearestShooterTarget(HexCoord from, PlayerId owner, HexCoord& out,
                                    int& outDist) const {
    // Bounded BFS outward from the (owner-owned) shooter cell; the first cell whose
    // owner differs is the nearest target. Frontier cells of `owner`'s blob are
    // reached at the distance to that frontier, so this naturally slows as the
    // claimed disc grows and stops once the frontier passes kShooterRange.
    std::unordered_set<int> seen;
    std::deque<std::pair<HexCoord, int>> queue;
    seen.insert(grid_.index(from));
    queue.emplace_back(from, 0);
    while (!queue.empty()) {
        const auto [c, dist] = queue.front();
        queue.pop_front();
        if (dist >= config::kShooterRange) {
            continue;
        }
        for (int d = 0; d < 6; ++d) {
            const HexCoord nb = neighbor(c, static_cast<HexDir>(d));
            if (!grid_.contains(nb) || seen.contains(grid_.index(nb))) {
                continue;
            }
            if (grid_.at(nb).owner != owner) {
                out = nb;
                outDist = dist + 1;
                return true;
            }
            seen.insert(grid_.index(nb));
            queue.emplace_back(nb, dist + 1);
        }
    }
    return false;
}

void HexWorld::updateShooters() {
    for (Shooter& s : shooters_) {
        const PlayerId owner = grid_.at(s.cell).owner;
        if (owner == kNeutral) {
            // Un-captured (or just lost to a death): idle, and ready to fire the
            // instant someone claims its cell.
            s.cooldown = 0.0F;
            continue;
        }
        HexCoord target{};
        int dist = 0;
        if (!nearestShooterTarget(s.cell, owner, target, dist)) {
            continue; // owner has claimed everything within reach
        }
        s.cooldown -= config::kFixedDt;
        if (s.cooldown <= 0.0F) {
            setOwner(target, owner); // annex one cell (neutral or stolen from a rival)
            s.target = target;
            s.shotCount += 1; // signal a shot -> the Scene spawns a single fading laser
            s.cooldown = config::shooterShotInterval(dist);
        }
    }
}

void HexWorld::advanceCellForTest() {
    resolveTick(forcedCellMoves(), false);
}

void HexWorld::resolveTick(const std::vector<Move>& moves, bool allowRespawn) {
    std::vector<char> dead(players_.size(), 0);
    std::vector<PlayerId> killer(players_.size(), kNeutral);
    detectDeaths(moves, dead, killer);
    for (std::size_t i = 0; i < players_.size(); ++i) {
        if (dead.at(i) != 0) {
            // A killer that itself fell this tick (mutual cut) can't inherit land,
            // so the victim's territory reverts to neutral in that case.
            PlayerId k = killer.at(i);
            if (k != kNeutral && dead.at(static_cast<std::size_t>(k)) != 0) {
                k = kNeutral;
            }
            killPlayer(players_.at(i).id, k);
        }
    }
    commitMoves(moves, dead);
    if (allowRespawn) {
        respawnDeadBots();
    }
}

std::vector<HexWorld::Move> HexWorld::integrateMotion() {
    std::vector<Move> moves(players_.size());
    constexpr float dt = config::kFixedDt;
    constexpr float maxTurn = config::kTurnRate * dt;
    for (std::size_t i = 0; i < players_.size(); ++i) {
        Player& p = players_.at(i);
        if (!p.alive) {
            continue;
        }
        // Curve toward the desired heading by at most one turn-rate step.
        const float delta = std::clamp(wrapAngle(p.desiredAngle - p.angle), -maxTurn, maxTurn);
        const float stepLen = (config::kHexSpacing / p.stepInterval) * dt;
        const float ang = deflectAngle(p.pos, p.angle + delta, stepLen); // slide off walls
        const Vec2 npos = p.pos + (unitFromAngle(ang) * stepLen);
        const HexCoord ncell = worldToAxial(npos, config::kHexSize);
        // A re-touch of the cell we just left (sub-cell jitter along an edge) is
        // not a real crossing — only count a genuinely new in-bounds cell.
        const bool entered = grid_.contains(ncell) && ncell != p.cell && ncell != p.fromCell;
        moves.at(i) = Move{.entered = entered,
                           .target = entered ? ncell : p.cell,
                           .heading = quantizeToHexDir(ang),
                           .angle = ang,
                           .pos = npos};
    }
    return moves;
}

std::vector<HexWorld::Move> HexWorld::forcedCellMoves() {
    // Deterministic test driver: force every alive player exactly one hex along its
    // desired axis (honoring the no-reversal and wall-deflection rules), so the
    // shared death/capture path can be exercised without the continuous integrator.
    std::vector<Move> moves(players_.size());
    for (std::size_t i = 0; i < players_.size(); ++i) {
        Player& p = players_.at(i);
        if (!p.alive) {
            continue;
        }
        HexDir h = p.heading;
        if (p.desiredDir != HexDir::None && p.desiredDir != opposite(p.heading)) {
            h = p.desiredDir;
        }
        h = deflectHeading(p.cell, h); // walls deflect, never kill
        const HexCoord target = neighbor(p.cell, h);
        moves.at(i) = Move{.entered = true,
                           .target = target,
                           .heading = h,
                           .angle = dirAngle(h),
                           .pos = axialToWorld(target, config::kHexSize)};
    }
    return moves;
}

float HexWorld::deflectAngle(Vec2 pos, float angle, float probe) const {
    const auto inBoundsAt = [&](float a) {
        return grid_.contains(worldToAxial(pos + (unitFromAngle(a) * probe), config::kHexSize));
    };
    if (inBoundsAt(angle)) {
        return angle;
    }
    // Fan out in 15° steps (turn the short way first) until a heading keeps the
    // forward step on the board — the continuous analogue of deflectHeading.
    constexpr float kInc = std::numbers::pi_v<float> / 12.0F;
    for (int s = 1; s <= 12; ++s) {
        for (const int sgn : {1, -1}) {
            const float a = angle + (static_cast<float>(sgn) * kInc * static_cast<float>(s));
            if (inBoundsAt(a)) {
                return a;
            }
        }
    }
    return angle; // fully boxed in (shouldn't happen on a real board)
}

void HexWorld::detectDeaths(const std::vector<Move>& moves, std::vector<char>& dead,
                            std::vector<PlayerId>& killer) {
    const std::size_t n = players_.size();
    // Trail cuts: crossing onto a trail kills its owner (your own trail kills you).
    for (std::size_t i = 0; i < n; ++i) {
        if (!moves.at(i).entered) {
            continue;
        }
        const HexCoord target = moves.at(i).target;
        const PlayerId trailOwner = grid_.at(target).trailOwner;
        if (trailOwner == kNoTrail) {
            continue;
        }
        if (trailOwner == players_.at(i).id) {
            // Brushing the freshest cell of your own trail is sub-cell jitter, not a
            // real self-cross; only crossing older trail is fatal.
            const std::vector<HexCoord>& trail = players_.at(i).trail;
            if (trail.empty() || target != trail.back()) {
                dead.at(i) = 1; // self-cut: no aggressor, killer stays kNeutral
            }
        } else {
            dead.at(static_cast<std::size_t>(trailOwner)) = 1;
            killer.at(static_cast<std::size_t>(trailOwner)) = players_.at(i).id; // the cutter
            players_.at(i).kills += 1;                                           // credit the cutter
        }
    }
    // Head-to-head: two avatars crossing into the same cell this tick both fall.
    for (std::size_t i = 0; i < n; ++i) {
        if (!moves.at(i).entered) {
            continue;
        }
        for (std::size_t j = i + 1; j < n; ++j) {
            if (moves.at(j).entered && moves.at(i).target == moves.at(j).target) {
                dead.at(i) = 1;
                dead.at(j) = 1;
            }
        }
    }
}

void HexWorld::commitMoves(const std::vector<Move>& moves, const std::vector<char>& dead) {
    for (std::size_t i = 0; i < players_.size(); ++i) {
        Player& p = players_.at(i);
        if (!p.alive || dead.at(i) != 0) {
            continue; // not moving, or died this tick — leave it where it fell
        }
        // Always advance the continuous pose so the avatar glides smoothly.
        p.pos = moves.at(i).pos;
        p.angle = moves.at(i).angle;
        p.heading = moves.at(i).heading;
        if (!moves.at(i).entered) {
            continue; // still within the same hex; nothing claimed
        }
        p.fromCell = p.cell;
        p.cell = moves.at(i).target;
        Cell& tc = grid_.at(p.cell);
        if (tc.owner == p.id) {
            if (!p.trail.empty()) {
                closeTrailAndCapture(p);
            }
        } else if (tc.trailOwner != p.id) {
            // Lay trail on fresh ground (a rival's trail here was already cleared by
            // the death pass; never double-record a cell already in our own trail).
            tc.trailOwner = p.id;
            p.trail.push_back(p.cell);
        }
        // Speed/Vision are collected on contact; the Shooter is a fixed item that
        // is never picked up (it acts for whoever owns its cell), so step over it.
        if (tc.powerup != 0 && tc.powerup != static_cast<std::uint8_t>(PowerUp::Shooter)) {
            applyPowerup(p, static_cast<PowerUp>(tc.powerup));
            tc.powerup = 0;
            if (activePowerups_ > 0) {
                --activePowerups_;
            }
        }
    }
}

void HexWorld::respawnDeadBots() {
    // Bots come back at a fresh clear spot; the human (id 0) stays dead.
    for (Player& p : players_) {
        if (p.isBot && !p.alive) {
            respawnBot(p.id);
        }
    }
}

void HexWorld::respawnBot(PlayerId id) {
    Player& p = players_.at(static_cast<std::size_t>(id));
    spawnHome(p, findSpawn(config::kHomeRadius + 1), config::kHomeRadius);
    p.desiredDir = HexDir::None;
}

HexCoord HexWorld::findSpawn(int clearRadius) {
    const int margin = clearRadius + 1;
    std::uniform_int_distribution<int> qd(margin, grid_.width() - 1 - margin);
    std::uniform_int_distribution<int> rd(margin, grid_.height() - 1 - margin);
    for (int attempt = 0; attempt < 200; ++attempt) {
        const HexCoord c{qd(rng_), rd(rng_)};
        bool clear = true;
        for (int q = c.q - clearRadius; q <= c.q + clearRadius && clear; ++q) {
            for (int r = c.r - clearRadius; r <= c.r + clearRadius && clear; ++r) {
                const HexCoord h{q, r};
                if (grid_.contains(h) && hexDistance(c, h) <= clearRadius &&
                    grid_.at(h).owner != kNeutral) {
                    clear = false;
                }
            }
        }
        if (clear) {
            return c;
        }
    }
    return {grid_.width() / 2, grid_.height() / 2}; // fallback (crowded map)
}

// ---- HexWorldView (read-only window for bots) -------------------------------

int HexWorldView::gridW() const {
    return world_->grid().width();
}
int HexWorldView::gridH() const {
    return world_->grid().height();
}
bool HexWorldView::inBounds(HexCoord c) const {
    return world_->grid().contains(c);
}
PlayerId HexWorldView::ownerAt(HexCoord c) const {
    return world_->grid().at(c).owner;
}
PlayerId HexWorldView::trailOwnerAt(HexCoord c) const {
    return world_->grid().at(c).trailOwner;
}
std::uint8_t HexWorldView::powerupAt(HexCoord c) const {
    return world_->grid().at(c).powerup;
}
int HexWorldView::playerCount() const {
    return static_cast<int>(world_->players().size());
}
bool HexWorldView::alive(PlayerId id) const {
    return world_->players().at(static_cast<std::size_t>(id)).alive;
}
HexCoord HexWorldView::cellOf(PlayerId id) const {
    return world_->players().at(static_cast<std::size_t>(id)).cell;
}
HexCoord HexWorldView::homeOf(PlayerId id) const {
    return world_->players().at(static_cast<std::size_t>(id)).home;
}
HexDir HexWorldView::headingOf(PlayerId id) const {
    return world_->players().at(static_cast<std::size_t>(id)).heading;
}
int HexWorldView::territoryCount(PlayerId id) const {
    return world_->players().at(static_cast<std::size_t>(id)).territoryCount;
}
std::span<const HexCoord> HexWorldView::trailOf(PlayerId id) const {
    return world_->players().at(static_cast<std::size_t>(id)).trail;
}

int HexWorldView::distanceToOwn(HexCoord from, PlayerId id, int maxRadius) const {
    const HexGrid& g = world_->grid();
    if (!g.contains(from)) {
        return maxRadius + 1;
    }
    if (g.at(from).owner == id) {
        return 0;
    }
    std::unordered_set<int> seen;
    std::deque<std::pair<HexCoord, int>> queue;
    seen.insert(g.index(from));
    queue.emplace_back(from, 0);
    while (!queue.empty()) {
        const auto [c, dist] = queue.front();
        queue.pop_front();
        if (dist >= maxRadius) {
            continue;
        }
        for (int d = 0; d < 6; ++d) {
            const HexCoord nb = neighbor(c, static_cast<HexDir>(d));
            if (!g.contains(nb) || seen.contains(g.index(nb))) {
                continue;
            }
            if (g.at(nb).owner == id) {
                return dist + 1;
            }
            seen.insert(g.index(nb));
            queue.emplace_back(nb, dist + 1);
        }
    }
    return maxRadius + 1;
}

void HexWorld::closeTrailAndCapture(Player& p) {
    if (p.trail.empty()) {
        return;
    }
    // 1. The trail itself becomes owned land.
    for (const HexCoord c : p.trail) {
        setOwner(c, p.id);
        grid_.at(c).trailOwner = kNoTrail;
    }

    // 2. Flood "outside" from the whole grid border through any cell NOT owned by
    // P. P's territory (existing land + the just-claimed trail) blocks the flood,
    // so any pocket it seals off is never reached. Closing against existing
    // territory works because that territory is part of the blocking set — which is
    // why this floods the whole grid rather than just the trail's bounding box.
    std::ranges::fill(visited_, std::uint8_t{0});
    std::vector<HexCoord> stack;
    const auto idx = [&](HexCoord c) { return static_cast<std::size_t>(grid_.index(c)); };
    const auto seed = [&](HexCoord c) {
        if (grid_.at(c).owner != p.id && visited_.at(idx(c)) == 0) {
            visited_.at(idx(c)) = 1;
            stack.push_back(c);
        }
    };
    for (int q = 0; q < grid_.width(); ++q) {
        seed({q, 0});
        seed({q, grid_.height() - 1});
    }
    for (int r = 0; r < grid_.height(); ++r) {
        seed({0, r});
        seed({grid_.width() - 1, r});
    }
    while (!stack.empty()) {
        const HexCoord c = stack.back();
        stack.pop_back();
        for (int d = 0; d < 6; ++d) {
            const HexCoord nb = neighbor(c, static_cast<HexDir>(d));
            if (grid_.contains(nb) && grid_.at(nb).owner != p.id && visited_.at(idx(nb)) == 0) {
                visited_.at(idx(nb)) = 1;
                stack.push_back(nb);
            }
        }
    }

    // 3. Any cell the flood never reached is enclosed — capture it.
    for (int i = 0; i < grid_.cellCount(); ++i) {
        if (grid_.cells().at(static_cast<std::size_t>(i)).owner != p.id &&
            visited_.at(static_cast<std::size_t>(i)) == 0) {
            setOwner(grid_.fromIndex(i), p.id);
        }
    }
    p.trail.clear();
}

void HexWorld::killPlayer(PlayerId id, PlayerId killer) {
    Player& d = players_.at(static_cast<std::size_t>(id));
    if (!d.alive) {
        return;
    }
    for (const HexCoord c : d.trail) {
        grid_.at(c).trailOwner = kNoTrail;
    }
    d.trail.clear();
    // The aggressor that cut this player down annexes its whole territory (the
    // reward for a kill); a death with no surviving killer — self-cut or
    // head-to-head — instead frees the land to neutral to keep the map churning.
    // Infrequent, so an O(cells) sweep is fine.
    const bool annex = killer != kNeutral && killer != id;
    for (Cell& cell : grid_.cells()) {
        if (cell.owner == id) {
            cell.owner = annex ? killer : kNeutral;
        }
    }
    if (annex) {
        players_.at(static_cast<std::size_t>(killer)).territoryCount += d.territoryCount;
    }
    d.territoryCount = 0;
    d.alive = false;
    // Phase C: bots respawn here; the human (id 0) stays dead -> Scene game-over.
}

void HexWorld::placePlayerForTest(PlayerId id, HexCoord cell, HexDir heading) {
    Player& p = players_.at(static_cast<std::size_t>(id));
    p.cell = cell;
    p.fromCell = cell;
    p.pos = axialToWorld(cell, config::kHexSize);
    p.angle = dirAngle(heading);
    p.desiredAngle = p.angle;
    p.heading = heading;
    p.desiredDir = HexDir::None;
    p.trail.clear();
}

} // namespace og::hexanaut
