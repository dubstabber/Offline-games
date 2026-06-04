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

// Shove every bot except `keep` into a corner and make it invulnerable + tiny,
// so only the snakes under test can interact.
void isolate(SnakeWorld& world, std::size_t keep) {
    for (std::size_t i = 1; i < world.snakes().size(); ++i) {
        if (i == keep) {
            continue;
        }
        Snake& b = world.snakeRef(i);
        b.head = {60.0F, 60.0F};
        b.path.clear();
        b.path.push_back(b.head);
        b.invulnTimer = 9999.0F;
        b.radius = 6.0F;
        b.spacing = 3.0F;
        b.alive = true;
    }
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
    p.path.clear();
    p.path.push_back(p.head); // isolate the one-sided hit (player head into bot body)

    const std::size_t foodBefore = world.food().size();
    world.step();
    assert(!world.playerAlive());
    assert(world.food().size() > foodBefore); // body became food
}

// A bot that crashes into the player's body dies and immediately respawns, so
// the population stays constant.
void testBotIntoPlayerRespawns() {
    SnakeWorld world(1, 88U);
    world.clearFood();
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
    b.invulnTimer = 5.0F; // spawn grace must not let it phase through bodies
    b.alive = true;
    b.score = 50.0F;
    b.radius = SnakeWorld::radiusForScore(50.0F);
    b.spacing = b.radius * cfg::kSpacingFactor;
    b.head = p.path.at(20); // sitting on the player's body
    b.path.clear();
    b.path.push_back(b.head); // isolate the one-sided hit (bot head into player body)

    assert(world.liveSnakeCount() == cfg::kBotCount + 1);
    world.step();
    assert(world.playerAlive());                          // only the bot drove into a body
    assert(world.liveSnakeCount() == cfg::kBotCount + 1); // bot respawned
    assert(world.snakeRef(2).alive);
    assert(static_cast<int>(world.food().size()) > cfg::kFoodMaxSpawnPerStep);
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

// Spawn grace protects against the border only; bodies remain solid so freshly
// spawned small snakes cannot pass through another snake's tail.
void testInvulnerabilityProtectsBorderOnly() {
    SnakeWorld borderWorld(1, 909U);
    borderWorld.clearFood();
    isolate(borderWorld, borderWorld.snakes().size());
    Snake& borderPlayer = borderWorld.snakeRef(0);
    borderPlayer.invulnTimer = 5.0F;
    borderPlayer.head = {borderPlayer.radius * 0.25F, 2500.0F};
    borderPlayer.path.clear();
    borderPlayer.path.push_back(borderPlayer.head);
    borderWorld.setPlayerInput({.aimWorld = borderPlayer.head, .boost = false});
    borderWorld.step();
    assert(borderWorld.playerAlive());

    SnakeWorld bodyWorld(1, 910U);
    bodyWorld.clearFood();
    isolate(bodyWorld, 1);
    Snake& b = bodyWorld.snakeRef(1);
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
    Snake& p = bodyWorld.snakeRef(0);
    p.invulnTimer = 5.0F; // border-protected, but bodies are still lethal
    p.head = b.path.at(10);
    p.path.clear();
    p.path.push_back(p.head); // isolate the one-sided hit (player head into bot body)
    bodyWorld.setPlayerInput({.aimWorld = p.head, .boost = false});
    bodyWorld.step();
    assert(!bodyWorld.playerAlive());
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

// Regression: a snake whose stored samples are spaced wider than its *current*
// spacing (as after boosting shrinks it) must collide along its whole tail, not
// just the part near the head. The old `path.size() * spacing` broad-phase
// under-counted the body and silently dropped far-tail collisions.
void testTailCollisionAfterShrink() {
    SnakeWorld world(1, 31U);
    isolate(world, 1);
    Snake& e = world.snakeRef(1);
    e.alive = true;
    e.invulnTimer = 9999.0F; // only the player can die here
    e.score = 80.0F;
    e.radius = SnakeWorld::radiusForScore(80.0F);
    e.spacing = e.radius * cfg::kSpacingFactor; // small *current* spacing
    e.head = {2500.0F, 2500.0F};
    e.heading = 0.0F;
    e.path.clear();
    const int n = SnakeWorld::segmentCountForScore(80.0F);
    const float histSpacing = cfg::kRadiusCap * cfg::kSpacingFactor; // wide, pre-shrink
    for (int k = 0; k < n; ++k) {
        e.path.emplace_back(2500.0F - (static_cast<float>(k) * histSpacing), 2500.0F);
    }
    // Drop the player onto the FAR tail — exactly what the old code missed.
    Snake& p = world.snakeRef(0);
    p.invulnTimer = 0.0F;
    p.score = 40.0F;
    p.radius = SnakeWorld::radiusForScore(40.0F);
    p.spacing = p.radius * cfg::kSpacingFactor;
    p.head = e.path.at(static_cast<std::size_t>(n - 1));
    p.path.clear();
    p.path.push_back(p.head); // isolate the one-sided hit (player head into enemy tail)
    world.setPlayerInput({.aimWorld = p.head, .boost = false});
    world.step();
    assert(!world.playerAlive());
}

// A long, fat, invulnerable wall along +x from `at`, never moving or dying.
void buildWall(SnakeWorld& world, std::size_t index, float score, Vec2 at) {
    Snake& s = world.snakeRef(index);
    s.alive = true;
    s.invulnTimer = 9999.0F;
    s.score = score;
    s.radius = SnakeWorld::radiusForScore(score);
    s.spacing = s.radius * cfg::kSpacingFactor;
    s.head = at;
    s.heading = 0.0F;
    s.path.clear();
    const int n = SnakeWorld::segmentCountForScore(score);
    for (int k = 0; k < n; ++k) {
        s.path.emplace_back(at.x + (static_cast<float>(k) * s.spacing), at.y);
    }
}

// Regression for the reported pass-through: a player that steers across another
// snake's body must die on contact, not slip through it. We drive the player
// straight across a fat wall at several points (including the neck region the old
// arc-exclusion wrongly let through) and require it to die before reaching the
// far side.
void testCannotPassThroughBody() {
    for (int xStep = 0; xStep <= 4; ++xStep) {
        const float crossX = 2520.0F + (static_cast<float>(xStep) * 40.0F);
        SnakeWorld world(1, 7U);
        world.clearFood();
        isolate(world, 1);
        buildWall(world, 1, 800.0F, {2500.0F, 2500.0F});
        Snake& p = world.snakeRef(0);
        p.alive = true;
        p.invulnTimer = 0.0F;
        p.score = 30.0F;
        p.radius = SnakeWorld::radiusForScore(30.0F);
        p.spacing = p.radius * cfg::kSpacingFactor;
        p.head = {crossX, 2400.0F};
        p.heading = std::numbers::pi_v<float> * 0.5F; // facing +y, toward the wall
        p.path.clear();
        p.path.push_back(p.head);
        bool passedThrough = false;
        for (int i = 0; i < 60; ++i) {
            world.setPlayerInput({.aimWorld = {crossX, 2700.0F}, .boost = false});
            world.step();
            if (!world.playerAlive()) {
                break;
            }
            if (world.player().head.y > 2560.0F) { // got to the far side alive
                passedThrough = true;
                break;
            }
        }
        assert(!passedThrough);
        assert(!world.playerAlive());
    }
}

// Regression for the reported "small snakes slip through": a BIG snake whose head
// sweeps over a much SMALLER snake (whose whole body fits inside the big head's
// reach, so both heads "overlap") must not sail through unharmed. The small enemy
// crosses the player's path rather than facing off, so it is a crash, not a
// head-on: if the player overlaps it, the player must die.
void testBigSnakeCannotOverrunSmall() {
    for (int t = 0; t < 8; ++t) {
        SnakeWorld world(1, static_cast<std::uint32_t>(300 + t));
        world.clearFood();
        isolate(world, 1);
        const Vec2 c{2500.0F, 2500.0F};

        Snake& p = world.snakeRef(0);
        p.alive = true;
        p.invulnTimer = 0.0F;
        p.score = 894.0F;
        p.radius = SnakeWorld::radiusForScore(894.0F);
        p.spacing = p.radius * cfg::kSpacingFactor;
        p.head = c;
        p.heading = 0.0F; // facing +x
        p.path.clear();
        for (int k = 0; k < SnakeWorld::segmentCountForScore(894.0F); ++k) {
            p.path.emplace_back(c.x - (static_cast<float>(k) * p.spacing), c.y);
        }

        Snake& e = world.snakeRef(1);
        e.alive = true;
        e.invulnTimer = 0.0F;
        e.score = 50.0F;
        e.radius = SnakeWorld::radiusForScore(50.0F);
        e.spacing = e.radius * cfg::kSpacingFactor;
        const float ex = c.x + p.radius + 30.0F + (static_cast<float>(t) * 8.0F);
        e.head = {ex, c.y + 40.0F};
        e.heading = std::numbers::pi_v<float> * 0.5F; // crossing (+y), not facing the player
        e.path.clear();
        for (int k = 0; k < SnakeWorld::segmentCountForScore(50.0F); ++k) {
            e.path.emplace_back(ex, e.head.y - (static_cast<float>(k) * e.spacing));
        }

        bool overlapped = false;
        bool died = false;
        for (int i = 0; i < 80; ++i) {
            const Snake& ps = world.player();
            const Snake& es = world.snakeRef(1);
            const float rr = ps.radius + es.radius;
            if (es.alive) {
                for (const Vec2& seg : es.path) {
                    overlapped = overlapped || dist(ps.head, seg) < rr;
                }
            }
            world.setPlayerInput({.aimWorld = {c.x + 2000.0F, c.y}, .boost = false});
            world.step();
            if (!world.playerAlive()) {
                died = true;
                break;
            }
            if (world.player().head.x > ex + 250.0F) {
                break;
            }
        }
        assert(!overlapped || died); // overlapped a body => must have crashed
    }
}

// Regression for the coiled-tail case: a coiled player sitting still while a
// bigger enemy charges in and rams its tail must survive — the enemy drove its
// head in, so the enemy dies, not the (smaller) sitting player. The enemy starts
// clear of the player and charges in over several steps (so it genuinely "drives
// in", which is how fault is assigned for an overlap of two heads).
void testSittingPlayerSurvivesCharger() {
    SnakeWorld world(1, 123U);
    world.clearFood();
    isolate(world, 1);
    const Vec2 C{2500.0F, 2500.0F};

    Snake& p = world.snakeRef(0);
    p.alive = true;
    p.invulnTimer = 0.0F;
    p.score = 52.0F;
    p.radius = SnakeWorld::radiusForScore(52.0F);
    p.spacing = p.radius * cfg::kSpacingFactor;
    const int n = SnakeWorld::segmentCountForScore(52.0F);
    const float twoPi = 2.0F * std::numbers::pi_v<float>;
    const float r0 = (static_cast<float>(n) * p.spacing) / twoPi;
    p.path.clear();
    for (int k = 0; k < n; ++k) {
        const float ang = static_cast<float>(k) * twoPi / static_cast<float>(n);
        p.path.emplace_back(C.x + (r0 * std::cos(ang)), C.y + (r0 * std::sin(ang)));
    }
    p.head = p.path.front();
    p.heading = std::numbers::pi_v<float> * 0.5F;

    Snake& e = world.snakeRef(1);
    e.alive = true;
    e.invulnTimer = 0.0F;
    e.score = 300.0F;
    e.radius = SnakeWorld::radiusForScore(300.0F);
    e.spacing = e.radius * cfg::kSpacingFactor;
    const Vec2 outward = og::snake::normalize(p.path.back() - C);
    e.head = p.path.back() + (outward * 140.0F); // starts clear, will charge in
    const int en = SnakeWorld::segmentCountForScore(300.0F);
    e.path.clear();
    for (int k = 0; k < en; ++k) {
        e.path.push_back(e.head + (outward * (static_cast<float>(k) * e.spacing)));
    }
    e.heading = og::snake::angleOf(C - e.head);
    e.bot.aggressiveness = 1.0F;

    bool enemyDied = false;
    float prevEnemyScore = e.score;
    for (int i = 0; i < 60; ++i) {
        world.setPlayerInput({.aimWorld = world.player().head, .boost = false});
        world.step();
        if (world.snakeRef(1).score < prevEnemyScore - 50.0F) {
            enemyDied = true; // respawned much smaller
        }
        prevEnemyScore = world.snakeRef(1).score;
        assert(world.playerAlive()); // the sitting player is never blamed
    }
    assert(enemyDied); // the charger crashed into the player and respawned
}

// Regression: in a true head-on (both heads driving into each other) only ONE
// snake should die — the smaller — not both. The player (small) charges a bigger
// aggressive enemy head-on; the player dies and the enemy lives.
void testHeadOnKillsOnlySmaller() {
    SnakeWorld world(1, 51U);
    world.clearFood();
    isolate(world, 1);
    const Vec2 c{2500.0F, 2500.0F};

    Snake& p = world.snakeRef(0);
    p.alive = true;
    p.invulnTimer = 0.0F;
    p.score = 40.0F;
    p.radius = SnakeWorld::radiusForScore(40.0F);
    p.spacing = p.radius * cfg::kSpacingFactor;
    p.head = c;
    p.heading = 0.0F; // facing +x
    p.path.clear();
    for (int k = 0; k < SnakeWorld::segmentCountForScore(40.0F); ++k) {
        p.path.emplace_back(c.x - (static_cast<float>(k) * p.spacing), c.y);
    }

    Snake& e = world.snakeRef(1);
    e.alive = true;
    e.invulnTimer = 0.0F;
    e.score = 600.0F;
    e.radius = SnakeWorld::radiusForScore(600.0F);
    e.spacing = e.radius * cfg::kSpacingFactor;
    e.head = {c.x + 400.0F, c.y};
    e.heading = std::numbers::pi_v<float>; // facing -x, toward the player
    e.path.clear();
    for (int k = 0; k < SnakeWorld::segmentCountForScore(600.0F); ++k) {
        e.path.emplace_back(e.head.x + (static_cast<float>(k) * e.spacing), e.head.y);
    }
    e.bot.aggressiveness = 1.0F;

    for (int i = 0; i < 80 && world.playerAlive(); ++i) {
        world.setPlayerInput({.aimWorld = {c.x + 1000.0F, c.y}, .boost = false}); // charge +x
        world.step();
    }
    assert(!world.playerAlive());             // the smaller snake dies
    assert(world.snakeRef(1).alive);          // the bigger one lives -> NOT both
    assert(world.snakeRef(1).score > 400.0F); // it did not die and respawn small
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
    testTailCollisionAfterShrink();
    testCannotPassThroughBody();
    testBigSnakeCannotOverrunSmall();
    testSittingPlayerSurvivesCharger();
    testHeadOnKillsOnlySmaller();
    testDeterminism();
    testFoodRespawnKeepsCount();
    testInvulnerabilityProtectsBorderOnly();
    testGeometryBounds();
    testGhostLeaderboard();
    testGhostDeterministicAndBounded();
    std::puts("All Snake tests passed.");
    return 0;
}
