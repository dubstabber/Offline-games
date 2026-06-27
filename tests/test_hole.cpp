#include "games/hole/HoleConfig.hpp"
#include "games/hole/HoleTypes.hpp"
#include "games/hole/HoleWorld.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

using og::hole::CityObject;
using og::hole::HolePlayer;
using og::hole::HoleWorld;
using og::hole::ObjectKind;
using og::hole::PlayerInput;
using og::hole::Vec2;
namespace cfg = og::hole::config;

bool nearly(float a, float b, float eps) {
    return std::fabs(a - b) < eps;
}

float speedOf(const HoleWorld& world) {
    return og::hole::length(world.player().vel);
}

void placePlayer(HoleWorld& world, Vec2 pos, float score = 0.0F) {
    world.playerRef().pos = pos;
    world.playerRef().vel = {0.0F, 0.0F};
    world.playerRef().score = score;
    world.playerRef().alive = true;
    world.playerRef().graceTimer = 0.0F;
    world.refreshPlayerRadius();
}

void isolatePlayer(HoleWorld& world, Vec2 pos, float score = 0.0F) {
    world.clearObjects();
    world.clearBotsForTest();
    placePlayer(world, pos, score);
}

CityObject object(ObjectKind kind, Vec2 pos, float mass, float requiredRadius) {
    const cfg::ObjectSpec& spec = cfg::specFor(kind);
    CityObject out;
    out.kind = kind;
    out.pos = pos;
    out.pathA = pos;
    out.pathB = pos;
    out.mass = mass;
    out.solidRadius = spec.solidRadius;
    out.consumeRadius = spec.consumeRadius;
    out.requiredRadius = requiredRadius;
    return out;
}

void stepMany(HoleWorld& world, int count) {
    for (int i = 0; i < count; ++i) {
        world.step();
    }
}

void testDifficultyProfiles() {
    HoleWorld easy(0, 100U);
    HoleWorld medium(1, 100U);
    HoleWorld hard(2, 100U);
    assert(nearly(easy.worldW(), 4500.0F, 0.01F));
    assert(nearly(medium.worldW(), 5400.0F, 0.01F));
    assert(nearly(hard.worldW(), 6300.0F, 0.01F));
    assert(easy.districtCount() == 5);
    assert(medium.districtCount() == 6);
    assert(hard.districtCount() == 7);
    assert(easy.botCount() == 4);
    assert(medium.botCount() == 6);
    assert(hard.botCount() == 8);
    assert(easy.holes().size() == 5U);
    assert(medium.holes().size() == 7U);
    assert(hard.holes().size() == 9U);
    assert(easy.totalObjectCount() < medium.totalObjectCount());
    assert(medium.totalObjectCount() < hard.totalObjectCount());
}

void testDragMovesTowardAim() {
    HoleWorld world(1, 1U);
    isolatePlayer(world, {500.0F, 500.0F});
    for (int i = 0; i < 60; ++i) {
        world.setPlayerInput(PlayerInput{.aimWorld = {1200.0F, 500.0F}, .active = true});
        world.step();
    }
    assert(world.player().pos.x > 770.0F);
    assert(nearly(world.player().pos.y, 500.0F, 2.0F));
}

void testReleaseAppliesFriction() {
    HoleWorld world(1, 2U);
    isolatePlayer(world, {500.0F, 500.0F});
    for (int i = 0; i < 25; ++i) {
        world.setPlayerInput(PlayerInput{.aimWorld = {1200.0F, 500.0F}, .active = true});
        world.step();
    }
    const float moving = speedOf(world);
    for (int i = 0; i < 90; ++i) {
        world.setPlayerInput(PlayerInput{.aimWorld = world.player().pos, .active = false});
        world.step();
    }
    assert(moving > 150.0F);
    assert(speedOf(world) < moving * 0.1F);
}

void testEatObjectGrowsAndCompletes() {
    HoleWorld world(1, 3U);
    isolatePlayer(world, {600.0F, 600.0F});
    const float before = world.player().radius;
    world.addObject(object(ObjectKind::Cone, {600.0F, 600.0F}, 100.0F, 1.0F));
    world.step();
    assert(world.objects().front().consumed);
    assert(world.playerScore() == 100);
    assert(world.player().radius > before);
    assert(world.completed());
    assert(world.finished());
    assert(nearly(world.completionPercent(), 100.0F, 0.01F));
}

void testTooLargeObjectBlocksInsteadOfEating() {
    HoleWorld world(1, 4U);
    isolatePlayer(world, {500.0F, 500.0F});
    CityObject blocker = object(ObjectKind::Car, {540.0F, 500.0F}, 25.0F, 999.0F);
    blocker.solidRadius = 58.0F;
    world.addObject(blocker);
    world.step();
    assert(!world.objects().front().consumed);
    assert(world.playerScore() == 0);
    assert(world.player().pos.x < 500.0F);
}

void testCompletionPercentTracksMass() {
    HoleWorld world(1, 5U);
    isolatePlayer(world, {300.0F, 300.0F}, 400.0F);
    world.addObject(object(ObjectKind::Cone, {300.0F, 300.0F}, 10.0F, 1.0F));
    world.addObject(object(ObjectKind::Cone, {900.0F, 900.0F}, 30.0F, 1.0F));
    world.step();
    assert(nearly(world.completionPercent(), 25.0F, 0.01F));
    assert(!world.completed());

    placePlayer(world, {900.0F, 900.0F}, world.player().score);
    world.step();
    assert(world.completed());
    assert(nearly(world.completionPercent(), 100.0F, 0.01F));
}

void testMobileObjectReflectsOnPath() {
    HoleWorld world(1, 6U);
    isolatePlayer(world, {1000.0F, 1000.0F});
    CityObject walker = object(ObjectKind::Pedestrian, {199.0F, 100.0F}, 3.0F, 999.0F);
    walker.mobile = true;
    walker.pathA = {100.0F, 100.0F};
    walker.pathB = {200.0F, 100.0F};
    walker.pathT = 0.99F;
    walker.pathDir = 1.0F;
    walker.speed = 200.0F;
    world.addObject(walker);
    world.step();
    assert(world.objects().front().pathDir < 0.0F);
    assert(world.objects().front().pathT <= 1.0F);
    assert(world.objects().front().pos.x <= 200.0F);
}

void testBoundsClampPlayer() {
    HoleWorld world(1, 7U);
    isolatePlayer(world, {40.0F, 40.0F});
    for (int i = 0; i < 120; ++i) {
        world.setPlayerInput(PlayerInput{.aimWorld = {-1000.0F, -1000.0F}, .active = true});
        world.step();
    }
    assert(world.player().pos.x >= 0.0F);
    assert(world.player().pos.y >= 0.0F);
    assert(world.player().pos.x <= world.worldW());
    assert(world.player().pos.y <= world.worldH());
}

void testDifficultyChangesMovementAndGrowth() {
    assert(HoleWorld::radiusForScore(0.0F, 0) > HoleWorld::radiusForScore(0.0F, 2));
    assert(HoleWorld::radiusForScore(400.0F, 0) > HoleWorld::radiusForScore(400.0F, 2));

    HoleWorld easy(0, 8U);
    HoleWorld hard(2, 8U);
    isolatePlayer(easy, {500.0F, 500.0F});
    isolatePlayer(hard, {500.0F, 500.0F});
    for (int i = 0; i < 60; ++i) {
        easy.setPlayerInput(PlayerInput{.aimWorld = {1200.0F, 500.0F}, .active = true});
        hard.setPlayerInput(PlayerInput{.aimWorld = {1200.0F, 500.0F}, .active = true});
        easy.step();
        hard.step();
    }
    assert(easy.player().pos.x > hard.player().pos.x);
}

void testTimerExpiresAt120Seconds() {
    HoleWorld world(1, 9U);
    isolatePlayer(world, {500.0F, 500.0F});
    world.addObject(object(ObjectKind::Tower, {4000.0F, 4000.0F}, 300.0F, 999.0F));
    assert(nearly(world.timeRemaining(), cfg::kRoundSeconds, 0.001F));
    stepMany(world, static_cast<int>(std::lround(cfg::kRoundSeconds / cfg::kFixedDt)));
    assert(world.timedOut());
    assert(world.finished());
    assert(nearly(world.timeRemaining(), 0.0F, 0.02F));
}

void testPlayerCanBeEatenByLargerRival() {
    HoleWorld world(1, 10U);
    world.clearObjects();
    placePlayer(world, {1000.0F, 1000.0F}, 0.0F);
    HolePlayer& bot = world.holeRef(1);
    bot.pos = {1000.0F, 1000.0F};
    bot.vel = {0.0F, 0.0F};
    bot.score = 1400.0F;
    bot.alive = true;
    bot.graceTimer = 0.0F;
    world.refreshHoleRadius(1);
    world.step();
    assert(!world.playerAlive());
    assert(world.finished());
}

void testBotRespawnsAfterBeingEaten() {
    HoleWorld world(1, 11U);
    world.clearObjects();
    placePlayer(world, {1000.0F, 1000.0F}, 1800.0F);
    const int before = world.playerScore();
    HolePlayer& bot = world.holeRef(1);
    bot.pos = {1000.0F, 1000.0F};
    bot.vel = {0.0F, 0.0F};
    bot.score = 20.0F;
    bot.alive = true;
    bot.graceTimer = 0.0F;
    world.refreshHoleRadius(1);
    world.step();
    assert(!world.holes().at(1).alive);
    assert(world.holes().at(1).respawnTimer > 0.0F);
    assert(world.playerScore() > before);

    stepMany(world, static_cast<int>((cfg::kBotRespawnSeconds + 0.1F) / cfg::kFixedDt));
    assert(world.holes().at(1).alive);
    assert(world.holes().at(1).score == 0.0F);
    assert(world.holes().at(1).graceTimer > 0.0F);
}

void testBotConsumesCityObjects() {
    HoleWorld world(1, 12U);
    world.clearObjects();
    placePlayer(world, {3000.0F, 3000.0F}, 0.0F);
    HolePlayer& bot = world.holeRef(1);
    bot.pos = {1000.0F, 1000.0F};
    bot.vel = {0.0F, 0.0F};
    bot.score = 500.0F;
    bot.alive = true;
    bot.graceTimer = 0.0F;
    world.refreshHoleRadius(1);
    world.addObject(object(ObjectKind::Bench, {1000.0F, 1000.0F}, 20.0F, 1.0F));
    world.step();
    assert(world.objects().front().consumed);
    assert(world.holes().at(1).score > 500.0F);
}

void testRankCountsBots() {
    HoleWorld world(1, 13U);
    world.clearObjects();
    placePlayer(world, {500.0F, 500.0F}, 100.0F);
    world.holeRef(1).score = 300.0F;
    world.holeRef(2).score = 200.0F;
    world.holeRef(3).score = 50.0F;
    assert(world.playerRank() == 3);
}

void testNewObjectGrowthOrder() {
    assert(cfg::specFor(ObjectKind::Motorcycle).requiredRadius >
           cfg::specFor(ObjectKind::Tree).requiredRadius);
    assert(cfg::specFor(ObjectKind::Bus).requiredRadius >
           cfg::specFor(ObjectKind::Van).requiredRadius);
    assert(cfg::specFor(ObjectKind::Skyscraper).requiredRadius >
           cfg::specFor(ObjectKind::Tower).requiredRadius);
    const float hardSkyscraper = cfg::specFor(ObjectKind::Skyscraper).requiredRadius *
                                 cfg::kDifficultyProfiles.at(2).requirementScale;
    assert(HoleWorld::radiusForScore(6000.0F, 2) >= hardSkyscraper);
}

void testDeterministicReplay() {
    HoleWorld a(1, 2026U);
    HoleWorld b(1, 2026U);
    for (int i = 0; i < 240; ++i) {
        const Vec2 aim{1600.0F + (static_cast<float>(i % 90) * 8.0F), 1300.0F};
        const bool active = (i % 45) != 0;
        a.setPlayerInput(PlayerInput{.aimWorld = aim, .active = active});
        b.setPlayerInput(PlayerInput{.aimWorld = aim, .active = active});
        a.step();
        b.step();
    }
    assert(a.player().pos.x == b.player().pos.x);
    assert(a.player().pos.y == b.player().pos.y);
    assert(a.player().score == b.player().score);
    assert(a.timeRemaining() == b.timeRemaining());
    assert(a.holes().size() == b.holes().size());
    for (std::size_t i = 0; i < a.holes().size(); ++i) {
        assert(a.holes().at(i).pos.x == b.holes().at(i).pos.x);
        assert(a.holes().at(i).pos.y == b.holes().at(i).pos.y);
        assert(a.holes().at(i).score == b.holes().at(i).score);
        assert(a.holes().at(i).alive == b.holes().at(i).alive);
    }
    assert(a.objects().size() == b.objects().size());
    for (std::size_t i = 0; i < a.objects().size(); ++i) {
        assert(a.objects().at(i).pos.x == b.objects().at(i).pos.x);
        assert(a.objects().at(i).pos.y == b.objects().at(i).pos.y);
        assert(a.objects().at(i).consumed == b.objects().at(i).consumed);
    }
}

} // namespace

int main() {
    testDifficultyProfiles();
    testDragMovesTowardAim();
    testReleaseAppliesFriction();
    testEatObjectGrowsAndCompletes();
    testTooLargeObjectBlocksInsteadOfEating();
    testCompletionPercentTracksMass();
    testMobileObjectReflectsOnPath();
    testBoundsClampPlayer();
    testDifficultyChangesMovementAndGrowth();
    testTimerExpiresAt120Seconds();
    testPlayerCanBeEatenByLargerRival();
    testBotRespawnsAfterBeingEaten();
    testBotConsumesCityObjects();
    testRankCountsBots();
    testNewObjectGrowthOrder();
    testDeterministicReplay();
    std::puts("All Hole tests passed.");
    return 0;
}
