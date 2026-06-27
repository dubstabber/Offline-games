#include "games/hole/HoleConfig.hpp"
#include "games/hole/HoleTypes.hpp"
#include "games/hole/HoleWorld.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

using og::hole::CityObject;
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
    world.refreshPlayerRadius();
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

void testDragMovesTowardAim() {
    HoleWorld world(1, 1U);
    world.clearObjects();
    placePlayer(world, {500.0F, 500.0F});
    for (int i = 0; i < 60; ++i) {
        world.setPlayerInput(PlayerInput{.aimWorld = {1200.0F, 500.0F}, .active = true});
        world.step();
    }
    assert(world.player().pos.x > 770.0F);
    assert(nearly(world.player().pos.y, 500.0F, 2.0F));
}

void testReleaseAppliesFriction() {
    HoleWorld world(1, 2U);
    world.clearObjects();
    placePlayer(world, {500.0F, 500.0F});
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
    world.clearObjects();
    placePlayer(world, {600.0F, 600.0F});
    const float before = world.player().radius;
    world.addObject(object(ObjectKind::Cone, {600.0F, 600.0F}, 100.0F, 1.0F));
    world.step();
    assert(world.objects().front().consumed);
    assert(world.playerScore() == 100);
    assert(world.player().radius > before);
    assert(world.completed());
    assert(nearly(world.completionPercent(), 100.0F, 0.01F));
}

void testTooLargeObjectBlocksInsteadOfEating() {
    HoleWorld world(1, 4U);
    world.clearObjects();
    placePlayer(world, {500.0F, 500.0F});
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
    world.clearObjects();
    placePlayer(world, {300.0F, 300.0F}, 400.0F);
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
    world.clearObjects();
    placePlayer(world, {1000.0F, 1000.0F});
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
    world.clearObjects();
    placePlayer(world, {40.0F, 40.0F});
    for (int i = 0; i < 120; ++i) {
        world.setPlayerInput(PlayerInput{.aimWorld = {-1000.0F, -1000.0F}, .active = true});
        world.step();
    }
    assert(world.player().pos.x >= 0.0F);
    assert(world.player().pos.y >= 0.0F);
    assert(world.player().pos.x <= HoleWorld::worldW());
    assert(world.player().pos.y <= HoleWorld::worldH());
}

void testDifficultyChangesMovementAndGrowth() {
    assert(HoleWorld::radiusForScore(0.0F, 0) > HoleWorld::radiusForScore(0.0F, 2));
    assert(HoleWorld::radiusForScore(400.0F, 0) > HoleWorld::radiusForScore(400.0F, 2));

    HoleWorld easy(0, 8U);
    HoleWorld hard(2, 8U);
    easy.clearObjects();
    hard.clearObjects();
    placePlayer(easy, {500.0F, 500.0F});
    placePlayer(hard, {500.0F, 500.0F});
    for (int i = 0; i < 60; ++i) {
        easy.setPlayerInput(PlayerInput{.aimWorld = {1200.0F, 500.0F}, .active = true});
        hard.setPlayerInput(PlayerInput{.aimWorld = {1200.0F, 500.0F}, .active = true});
        easy.step();
        hard.step();
    }
    assert(easy.player().pos.x > hard.player().pos.x);
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
    assert(a.objects().size() == b.objects().size());
    for (std::size_t i = 0; i < a.objects().size(); ++i) {
        assert(a.objects().at(i).pos.x == b.objects().at(i).pos.x);
        assert(a.objects().at(i).pos.y == b.objects().at(i).pos.y);
        assert(a.objects().at(i).consumed == b.objects().at(i).consumed);
    }
}

} // namespace

int main() {
    testDragMovesTowardAim();
    testReleaseAppliesFriction();
    testEatObjectGrowsAndCompletes();
    testTooLargeObjectBlocksInsteadOfEating();
    testCompletionPercentTracksMass();
    testMobileObjectReflectsOnPath();
    testBoundsClampPlayer();
    testDifficultyChangesMovementAndGrowth();
    testDeterministicReplay();
    std::puts("All Hole tests passed.");
    return 0;
}
