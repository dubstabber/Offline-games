#include "games/snake/GhostLeaderboard.hpp"
#include "games/snake/SnakeConfig.hpp"
#include "games/snake/SnakeTypes.hpp"
#include "games/snake/SnakeWorld.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <numbers>
#include <vector>

namespace {

using og::snake::FoodOrb;
using og::snake::GhostLeaderboard;
using og::snake::LeaderboardView;
using og::snake::RealEntry;
using og::snake::Snake;
using og::snake::SnakeWorld;
using og::snake::Vec2;
namespace cfg = og::snake::config;

bool nearly(float a, float b, float eps) {
    return std::fabs(a - b) < eps;
}

float dist(Vec2 a, Vec2 b) {
    return std::sqrt(og::snake::distSq(a, b));
}

// Aim the player straight along its current heading (a far colinear point keeps
// the heading exactly constant, so displacement is clean to assert).
void aimStraight(SnakeWorld& world, bool boost) {
    const Snake& p = world.player();
    const Vec2 aim = p.head + (og::snake::fromAngle(p.heading) * 1.0e6F);
    world.setPlayerInput({.aimWorld = aim, .boost = boost});
}

// The head advances at base speed along a fixed heading when aimed straight.
void testHeadMovesAlongHeading() {
    SnakeWorld world(1, 12345U);
    const Vec2 start = world.player().head;
    const float heading = world.player().heading;
    aimStraight(world, false);
    constexpr int kSteps = 30;
    for (int i = 0; i < kSteps; ++i) {
        world.step();
    }
    const Vec2 end = world.player().head;
    const float expected = cfg::kBaseSpeed * static_cast<float>(kSteps) * cfg::kFixedDt;
    assert(nearly(dist(start, end), expected, 2.0F));
    // Direction unchanged (moved along the original heading).
    const float moved = og::snake::angleOf(end - start);
    assert(nearly(og::snake::wrapAngle(moved - heading), 0.0F, 0.02F));
}

// One step turns the heading by at most kMaxTurnRate * dt toward the target.
void testTurnRateLimited() {
    SnakeWorld world(1, 222U);
    const float before = world.player().heading;
    const Vec2 head = world.player().head;
    const Vec2 behind = head + (og::snake::fromAngle(before + std::numbers::pi_v<float>) * 1.0e6F);
    world.setPlayerInput({.aimWorld = behind, .boost = false});
    world.step();
    const float delta = std::fabs(og::snake::wrapAngle(world.player().heading - before));
    const float maxStep = cfg::kMaxTurnRate * cfg::kFixedDt;
    assert(delta <= maxStep + 1.0e-4F);
    assert(delta > maxStep * 0.5F); // it actually turned
}

// Eating food raises the score and (with enough mass) the body length.
void testEatFoodGrows() {
    SnakeWorld world(1, 7U);
    const float scoreBefore = world.player().score;
    const int segBefore = SnakeWorld::segmentCountForScore(scoreBefore);

    const Snake& p = world.player();
    const Vec2 ahead = p.head + (og::snake::fromAngle(p.heading) * 35.0F);
    world.clearFood();
    FoodOrb orb;
    orb.pos = ahead;
    orb.mass = 50.0F;
    orb.radius = 16.0F;
    orb.colorIndex = 0;
    world.addFood(orb);

    aimStraight(world, false);
    for (int i = 0; i < 4; ++i) {
        world.step();
    }
    const float scoreAfter = world.player().score;
    assert(scoreAfter >= scoreBefore + 49.0F); // ate the 50-mass orb
    assert(SnakeWorld::segmentCountForScore(scoreAfter) > segBefore);
}

// The body trails the head: samples are roughly `spacing` apart and the count
// converges to segmentCountForScore once the trail fills.
void testBodyFollows() {
    SnakeWorld world(1, 99U);
    aimStraight(world, false);
    for (int i = 0; i < 120; ++i) {
        aimStraight(world, false);
        world.step();
    }
    const Snake& p = world.player();
    assert(p.path.size() >= 2);
    const float gap = dist(p.path.at(0), p.path.at(1));
    assert(gap > p.spacing * 0.4F && gap < p.spacing * 1.8F);
    assert(static_cast<int>(p.path.size()) == SnakeWorld::segmentCountForScore(p.score));
}

// Drive a deterministic horizontal wall body for a bot and place the player's
// head on it; one step kills the player and drops loot.
void testHeadIntoBotKillsPlayer() {
    SnakeWorld world(1, 55U);
    Snake& b = world.snakeRef(1);
    b.alive = true;
    b.invulnTimer = 9999.0F;
    b.score = 120.0F;
    b.radius = 30.0F;
    b.spacing = 15.0F;
    b.head = {2500.0F, 2500.0F};
    b.path.clear();
    for (int k = 0; k < 40; ++k) {
        b.path.emplace_back(2500.0F - (static_cast<float>(k) * 15.0F), 2500.0F);
    }
    Snake& p = world.snakeRef(0);
    p.invulnTimer = 0.0F;
    p.head = b.path.at(10);

    const std::size_t foodBefore = world.food().size();
    world.step();
    assert(!world.playerAlive());
    assert(world.food().size() > foodBefore); // body became food
}

// A bot that crashes into the player's body dies and immediately respawns, so
// the population stays constant.
void testBotIntoPlayerRespawns() {
    SnakeWorld world(1, 88U);
    Snake& p = world.snakeRef(0);
    p.invulnTimer = 9999.0F;
    p.score = 200.0F;
    p.radius = 44.0F;
    p.spacing = 20.0F;
    p.head = {2500.0F, 2500.0F};
    p.path.clear();
    for (int k = 0; k < 60; ++k) {
        p.path.emplace_back(2500.0F - (static_cast<float>(k) * 20.0F), 2500.0F);
    }
    Snake& b = world.snakeRef(2);
    b.invulnTimer = 0.0F;
    b.alive = true;
    b.head = p.path.at(20); // sitting on the player's body

    assert(world.liveSnakeCount() == cfg::kBotCount + 1);
    world.step();
    assert(world.playerAlive());                          // player was invulnerable
    assert(world.liveSnakeCount() == cfg::kBotCount + 1); // bot respawned
    assert(world.snakeRef(2).alive);
}

// Running into the world edge is fatal once invulnerability has worn off.
void testBorderDeath() {
    SnakeWorld world(1, 4U);
    Snake& p = world.snakeRef(0);
    p.invulnTimer = 0.0F;
    p.head = {world.worldSize() - 1.0F, world.worldSize() * 0.5F};
    world.setPlayerInput(
        {.aimWorld = {world.worldSize() + 1000.0F, world.worldSize() * 0.5F}, .boost = false});
    world.step();
    assert(!world.playerAlive());
}

// Boosting moves faster and bleeds score; not boosting holds speed and score.
void testBoostFasterAndBleeds() {
    const auto run = [](bool boost) {
        SnakeWorld world(1, 314U);
        world.snakeRef(0).score = 200.0F;
        const Vec2 start = world.player().head;
        for (int i = 0; i < 30; ++i) {
            aimStraight(world, boost);
            world.step();
        }
        return std::pair<float, float>{dist(start, world.player().head), world.player().score};
    };
    const auto plain = run(false);
    const auto boosted = run(true);
    assert(boosted.first > plain.first * 1.7F); // ~kBoostMultiplier faster
    assert(boosted.second < 200.0F);            // boosting bled mass
    assert(plain.second >= 200.0F - 1.0e-3F);   // no bleed without boost
}

// A snake never dies from its own body (only other snakes/edges kill).
void testNoSelfCollision() {
    SnakeWorld world(1, 71U);
    // Coil the player tightly around its own head.
    Snake& p = world.snakeRef(0);
    p.score = 80.0F;
    p.radius = 40.0F;
    p.spacing = 18.0F;
    p.invulnTimer = 0.0F;
    const Vec2 c{2500.0F, 2500.0F};
    p.head = c;
    p.path.clear();
    for (int k = 0; k < 40; ++k) {
        const float a = static_cast<float>(k) * 0.3F;
        p.path.emplace_back(c.x + (std::cos(a) * 20.0F), c.y + (std::sin(a) * 20.0F));
    }
    // Push every bot far away and keep them invulnerable so only self-collision
    // (which must NOT happen) could kill the player.
    for (std::size_t i = 1; i < world.snakes().size(); ++i) {
        Snake& b = world.snakeRef(i);
        b.head = {4900.0F, 100.0F};
        b.path.clear();
        b.path.push_back(b.head);
        b.invulnTimer = 9999.0F;
        b.radius = 10.0F;
        b.alive = true;
    }
    world.step();
    assert(world.playerAlive());
}

// Same seed + same input sequence -> identical simulation state.
void testDeterminism() {
    SnakeWorld a(1, 2024U);
    SnakeWorld b(1, 2024U);
    for (int i = 0; i < 600; ++i) {
        const Vec2 aim{3000.0F, 3000.0F};
        const bool boost = (i % 120) < 60;
        a.setPlayerInput({.aimWorld = aim, .boost = boost});
        b.setPlayerInput({.aimWorld = aim, .boost = boost});
        a.step();
        b.step();
    }
    assert(a.food().size() == b.food().size());
    assert(a.snakes().size() == b.snakes().size());
    for (std::size_t i = 0; i < a.snakes().size(); ++i) {
        assert(a.snakes().at(i).head.x == b.snakes().at(i).head.x);
        assert(a.snakes().at(i).head.y == b.snakes().at(i).head.y);
        assert(a.snakes().at(i).score == b.snakes().at(i).score);
    }
    double sumA = 0.0;
    double sumB = 0.0;
    for (const FoodOrb& f : a.food()) {
        sumA += static_cast<double>(f.pos.x) + static_cast<double>(f.pos.y);
    }
    for (const FoodOrb& f : b.food()) {
        sumB += static_cast<double>(f.pos.x) + static_cast<double>(f.pos.y);
    }
    assert(sumA == sumB);
}

// Food is continuously topped up and never collapses.
void testFoodRespawnKeepsCount() {
    SnakeWorld world(1, 17U);
    for (int i = 0; i < 400; ++i) {
        aimStraight(world, false);
        world.step();
    }
    assert(static_cast<int>(world.food().size()) >= cfg::kFoodTargetCount - 60);
    assert(static_cast<int>(world.food().size()) <= cfg::kFoodTargetCount + 400);
}

// Invulnerability protects a fresh spawn from a body it overlaps.
void testInvulnerabilityProtects() {
    SnakeWorld world(1, 909U);
    Snake& b = world.snakeRef(1);
    b.alive = true;
    b.invulnTimer = 9999.0F;
    b.score = 120.0F;
    b.radius = 30.0F;
    b.spacing = 15.0F;
    b.head = {2500.0F, 2500.0F};
    b.path.clear();
    for (int k = 0; k < 40; ++k) {
        b.path.emplace_back(2500.0F - (static_cast<float>(k) * 15.0F), 2500.0F);
    }
    Snake& p = world.snakeRef(0);
    p.invulnTimer = 5.0F; // protected
    p.head = b.path.at(10);
    world.step();
    assert(world.playerAlive());

    // Now drop the shield and re-overlap: it dies.
    Snake& p2 = world.snakeRef(0);
    p2.invulnTimer = 0.0F;
    p2.head = world.snakeRef(1).path.at(10);
    world.step();
    assert(!world.playerAlive());
}

// Length and radius are monotonic, bounded functions of score.
void testGeometryBounds() {
    assert(SnakeWorld::segmentCountForScore(0.0F) == cfg::kBaseSegments);
    assert(SnakeWorld::segmentCountForScore(1.0e9F) == cfg::kMaxSegments);
    assert(SnakeWorld::segmentCountForScore(100.0F) <= SnakeWorld::segmentCountForScore(1000.0F));
    assert(nearly(SnakeWorld::radiusForScore(0.0F), cfg::kBaseRadius, 0.01F));
    assert(nearly(SnakeWorld::radiusForScore(1.0e9F), cfg::kRadiusCap, 0.01F));
    assert(SnakeWorld::radiusForScore(100.0F) <= SnakeWorld::radiusForScore(1000.0F));
}

// The synthetic board stays sorted, ranks the player, and counts everyone.
void testGhostLeaderboard() {
    GhostLeaderboard g(7U, 220);
    const std::vector<RealEntry> none;
    const LeaderboardView low = g.build(9, "YOU", 40, none);
    assert(low.total == 221); // 220 ghosts + player
    assert(!low.top.empty());
    for (std::size_t i = 1; i < low.top.size(); ++i) {
        assert(low.top.at(i - 1).score >= low.top.at(i).score); // descending
    }
    assert(low.playerRank >= 1 && low.playerRank <= low.total);
    assert(low.playerRank > 1); // a fresh player isn't on top

    const LeaderboardView high = g.build(9, "YOU", 9999999, none);
    assert(high.playerRank == 1);
    assert(high.top.at(0).isPlayer);
}

// Ghost drift is deterministic per seed and stays within the score band.
void testGhostDeterministicAndBounded() {
    GhostLeaderboard a(99U, 120);
    GhostLeaderboard b(99U, 120);
    for (int i = 0; i < 200; ++i) {
        a.advance(0.016F);
        b.advance(0.016F);
    }
    const std::vector<RealEntry> none;
    const LeaderboardView va = a.build(9, "YOU", 100, none);
    const LeaderboardView vb = b.build(9, "YOU", 100, none);
    assert(va.top.size() == vb.top.size());
    for (std::size_t i = 0; i < va.top.size(); ++i) {
        assert(va.top.at(i).score == vb.top.at(i).score);
        assert(va.top.at(i).name == vb.top.at(i).name);
    }
    for (const og::snake::LeaderRow& row : va.top) {
        assert(row.score >= 0 && row.score <= 28000);
    }
}

} // namespace

int main() {
    testHeadMovesAlongHeading();
    testTurnRateLimited();
    testEatFoodGrows();
    testBodyFollows();
    testHeadIntoBotKillsPlayer();
    testBotIntoPlayerRespawns();
    testBorderDeath();
    testBoostFasterAndBleeds();
    testNoSelfCollision();
    testDeterminism();
    testFoodRespawnKeepsCount();
    testInvulnerabilityProtects();
    testGeometryBounds();
    testGhostLeaderboard();
    testGhostDeterministicAndBounded();
    std::puts("All Snake tests passed.");
    return 0;
}
